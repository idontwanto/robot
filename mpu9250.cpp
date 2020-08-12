
#include "mpu9250.h"
#include <iostream>
#include <iomanip>
#include <unistd.h>
#include <math.h>
#include <stdio.h>

using namespace std;

namespace samsRobot {

	/* constructor: initialise the device
	 * param i2cbus: the bus number, default is 1
	 * param i2caddress: the device address
	 */
	mpu9250::mpu9250(unsigned i2cbus, unsigned int i2caddress):
		i2cdev(i2cbus, i2caddress){
		this->i2caddress = i2caddress;
		// enable the lpf.
		// This makes both accel and gyro rate 1khz.
		this->enable_dlpf = true;

		// init our members
		this->accelX = 0;
		this->accelY = 0;
		this->accelZ = 0;
		this->pitch = 0.0f;
		this->roll = 0.0f;
		this->yaw = 0.0f;
		this->tempRaw = 0;
		this->temp = 0.0f;
		this->registers = NULL;

		this->accel_range = mpu9250::PLUSMINUS_2_G;
		this->gyro_range = mpu9250::PLUSMINUS_500;

		// initialize the device
		this->init();
		this->updateRegisters();
	}

	/* initialise the device
	 */
	void mpu9250::init(){
		if (this->enable_dlpf == true){
			// low pass filtered to approx 94hz bandwidth (close enough to 100hz)
			// this also makes sampling freq for both accel and gyro 1khz
			this->writeRegister(ADDR_CONFIG, CONFIG_DLPF_CFG);
		}
		// use a gyro as clock ref instead of internal oscillator
		this->writeRegister(ADDR_PWR_MGMT_1, PWR_MGMT_1_CLK_SRC);
		// set gyro range
		this->writeRegister(ADDR_GYRO_CONFIG, (unsigned char)gyro_range << 3);
		// set accel range
		this->writeRegister(ADDR_ACCEL_CONFIG, (unsigned char)accel_range << 3);
		// enable interrupts when data is ready
		this->writeRegister(ADDR_INT_ENABLE, 0x01);
	}

	/* read the sensor values. checks device can be correctly read,
	 * then read in up to date values, process and update class members
	 */
	int mpu9250::readSensorState(){
		if(this->readRegister(0x75) != DEV_ID){
			perror("MPU6050: Failure to read from correct device\n");
			return -1;
		}

		this->registers = this->readRegisters(DEV_NUM_REG, 0x0);

		this->accelX = this->combineRegisters(*(registers+ADDR_ACCEL_DATA_X_H), *(registers+ADDR_ACCEL_DATA_X_L));
		this->accelY = this->combineRegisters(*(registers+ADDR_ACCEL_DATA_Y_H), *(registers+ADDR_ACCEL_DATA_Y_L));
		this->accelZ = this->combineRegisters(*(registers+ADDR_ACCEL_DATA_Z_H), *(registers+ADDR_ACCEL_DATA_Z_L));

		this->gyroX = this->combineRegisters(*(registers+ADDR_GYRO_DATA_X_H), *(registers+ADDR_GYRO_DATA_X_L));
		this->gyroY = this->combineRegisters(*(registers+ADDR_GYRO_DATA_Y_H), *(registers+ADDR_GYRO_DATA_Y_L));
		this->gyroZ = this->combineRegisters(*(registers+ADDR_GYRO_DATA_Z_H), *(registers+ADDR_GYRO_DATA_Z_L));

		this->tempRaw = this->combineRegisters(*(registers+ADDR_TEMP_DATA_H), *(registers+ADDR_TEMP_DATA_L));

		// read in the sensors actual current range
		this->accel_range = (mpu9250::ACCEL_RANGE) ((*(registers + ADDR_ACCEL_CONFIG))&0x18);
		this->gyro_range = (mpu9250::GYRO_RANGE) ((*(registers + ADDR_GYRO_CONFIG))&0x18);

		this->calcPitchRollYaw();
		this->calcAngVel();
		this->calcTemp();
		return 0;
	}

	/* set range of accelerometer according to ACCEL_RANGE enum
	 * param range: one of enum values
	 */
	void mpu9250::setAccelRange(mpu9250::ACCEL_RANGE range)
	{
		this->accel_range = range;
		this->updateRegisters();
	}
	mpu9250::ACCEL_RANGE mpu9250::getAccelRange(void){
		return this->accel_range;
	}
	/* set range of gyroscope according to GYRO_RANGE enum
	 * param range: one of enum values
	 */
	void mpu9250::setGyroRange(mpu9250::GYRO_RANGE range){
		this->gyro_range = range;
		this->updateRegisters();
	}
	mpu9250::GYRO_RANGE mpu9250::getGyroRange(void){
		return this->gyro_range;
	}

	/* combine 8bit registers into a single short (16bit on rpi).
	 * param msb: unsigned char which is msb
	 * param lsb: unsigned char which is lsb
	 */
	short mpu9250::combineRegisters(unsigned char msb, unsigned char lsb){
		return ((short)msb<<8 | (short)lsb);
	}

	/* calculate pitch, roll and yaw values.
	 * Accounts for range/resolution and effect of gravity
	 */
	void mpu9250::calcPitchRollYaw()
	{
		float factor = 0.0f;

		switch(this->accel_range){
			case mpu9250::PLUSMINUS_16_G : factor = 2048.0f; break; // AFS_SEL = 0
			case mpu9250::PLUSMINUS_8_G : factor = 4096.0f; break; // AFS_SEL = 1
			case mpu9250::PLUSMINUS_4_G : factor = 8192.0f; break; // AFS_SEL = 2
			default: factor = 16384.0f; break; // AFS_SEL = 3
		}
		float accXg = this->accelX * factor;
		float accYg = this->accelY * factor;
		float accZg = this->accelZ * factor;
		float accXg_sq = accXg * accXg;
		float accYg_sq = accYg * accYg;
		float accZg_sq = accZg * accZg;

		this->pitch = 180.0f * atan(accXg/sqrt(accYg_sq + accZg_sq))/M_PI;
		this->roll = 180.0f * atan(accYg/sqrt(accXg_sq + accZg_sq))/M_PI;
		this->yaw = 180.0f * atan(accZg/sqrt(accXg_sq + accYg_sq))/M_PI;
	}

	/* calculate the position of the device
	 */
	void mpu9250::calcAngVel(){
		float factor = 0.0f;
		switch (this->gyro_range){
		case mpu9250::PLUSMINUS_2000 : factor = 16.4f; break; //FS_SEL = 0
		case mpu9250::PLUSMINUS_1000 : factor = 32.8f; break; //FS_SEL = 1
		case mpu9250::PLUSMINUS_500 : factor = 65.5f; break; //FS_SEL = 2
		default: factor = 131.0f; // FS_SEL = 3
		}
		this->wvelX = (float)gyroX/factor;
		this->wvelY = (float)gyroY/factor;
		this->wvelZ = (float)gyroZ/factor;
	}

	/* calculate the temperature using the raw data
	 */
	void mpu9250::calcTemp(){
		this->temp = ((float)(tempRaw)/TEMP_SENS) + (float)TEMP_OFFSET;
	}

	/* update the device registers as required
	 * return 0 if successful
	 */
	int mpu9250::updateRegisters(){
		// nothing specific here
		this->init();
		return 0;
	}

	void mpu9250::displayData(int iterations){
		int count = 0;
		while (count < iterations){
			cout << "@"<< count << "\tOrient PRY (deg):" << std::setprecision(3) << this->getPitch() << " " << this->getRoll() << " " << this->getYaw() <<" " <<endl;
			cout << "@"<< count << "\tAngVel XYZ (deg/sec): " << this->getAngVelX() << " " << this->getAngVelY() << " " << this->getAngVelZ() << " " << endl;
			cout << "@"<< count << "\tTemp (degC): " << this->getTemp()<< " raw :" << this->tempRaw << endl <<endl;
			usleep(100000); // 100ms
			this->readSensorState();
			count++;
		}
	}

	mpu9250::~mpu9250(){
		/* nothing to do here*/
	}

	short mpu9250::getAccelX(void){return this->accelX;}
	short mpu9250::getAccelY(void){return this->accelY;}
	short mpu9250::getAccelZ(void){return this->accelZ;}

	float mpu9250::getPitch(void){return this->pitch;}
	float mpu9250::getRoll(void){return this->roll;}
	float mpu9250::getYaw(void){return this->yaw;}

	float mpu9250::getAngVelX(void){return this->wvelX;}
	float mpu9250::getAngVelY(void){return this->wvelY;}
	float mpu9250::getAngVelZ(void){return this->wvelZ;}


	float mpu9250::getTemp(void){return this->temp;}

} // namespace


