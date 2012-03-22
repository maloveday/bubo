/*
 * RotorController.hpp
 *
 *  Created on: Mar 22, 2012
 *      Author: Mark Doyle
 */

#ifndef ROTORCONTROLLER_HPP_
#define ROTORCONTROLLER_HPP_

#include "Arduino.h"
#include "bubo/CommandListener.hpp"

namespace bubo {

/**
 * Conventions:
 * AZ or az = Azimuth
 * EL or el = Elevation
 */
class RotorController : public CommandListener {
	public:
		RotorController();
		~RotorController() {};

		void rotate();

		void allStop();

		void acceptCommand(Command cmd);

		unsigned long getRotorMoveUpdateInterval() const;

		void setRotorMoveUpdateInterval(unsigned long newRotorMoveUpdateInterval);

		long getAzimuthaAdZeroOffset() const {
			return this->azimuthaAdZeroOffset;
		}

		void setAzimuthaAdZeroOffset(long newOffset) {
			this->azimuthaAdZeroOffset = newOffset;
		}

		long getElevationAdZeroOffset() const {
			return this->elevationAdZeroOffset;
		}

		void setElevationAdZeroOffset(long newOffset) {
			this->elevationAdZeroOffset = newOffset;
		}

	private:
		/**
		 * 10 bit A/D converters in the Arduino have a max value of 1023
		 * for the azimuth the A/D value of 1023 should correspond to 450 degrees
		 * for the elevation the A/D value of 1023 should correspond to 180 degrees
		 * integer math is used so the scale value is multiplied by 100 to maintain accuracy
		 * the scale factor should be 100 * (1023 / 450) for the azimuth
		 * the scale factor should be 100 * (1023 / 180) for the elevation
		 */
		static const long AZ_SCALE_FACTOR;
		static const long EL_SCALE_FACTOR;

		/**
		 * A/D converter parameters
		 * AFTER you have adjusted your G-5500 control box as per the manual
		 * adjust the next 4 parameters. The settings interact a bit so you may have
		 * to go back and forth a few times. Remember the G-5500 rotors are not all that
		 * accurate (within 4 degrees at best) so try not to get too compulsive when
		 * making these adjustments.
		 *
		 */
		/** adjust to zero out lcd az reading when control box az = 0 */
		long azimuthaAdZeroOffset;
		/** adjust to zero out lcd el reading when control box el = 0 */
		long elevationAdZeroOffset;

		/** elevation rotor up control line */
		static const byte PIN_EL_UP;
		/** elevation rotor down control line */
		static const byte PIN_EL_DOWN;
		/** azimuth rotor left control line */
		static const byte PIN_AZ_LEFT;
		/** azimuth rotor right control line */
		static const byte PIN_AZ_RIGHT;

		/** maximum rotor azimuth in degrees * 100 */
		static const long MAX_AZIMUTH;
		/** maximum rotor elevation in degrees * 100 */
		static const long MAX_ELEVATION;

		/** The analogue input pin used to receive the current rotor azimuth voltage */
		static const byte PIN_AZ_INPUT;
		/** The analogue input pin used to receive the current rotor elevation voltage */
		static const byte PIN_EL_INPUT;

		/** Current elevation as received by the rotor on PIN_ELEVATION */
		long currentElevation;
		/** Current elevation as received by the rotor on PIN_AZIMUTH */
		long currentAzimuth;

		/**
		 * Tolerance for az-el match in rotor move in degrees * 100
		 * take care if you lower this value -  wear or dirt on the pots in your rotors
		 * or A/D converter jitter may cause hunting if the value is too low.
		 */
		long bias;

		unsigned long rotorMoveUpdateInterval;

		bool azimuthMove;
		bool elevationMove;

		long targetAzimuth;
		long targetElevation;

		void updateAzimuth();

		void updateElevation();

		void stopAzimuthRotor();

		void stopElevationRotor();

		bool isCommandValid(Command cmd);

		void rotateAzimuth();
		void rotateElevation();
};

}

#endif /* ROTORCONTROLLER_HPP_ */
