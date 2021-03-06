/*
 * BaseCommand.hpp
 *
 *  Created on: 9 May 2012
 *      Author: Mark Doyle
 */

#ifndef BASECOMMAND_H_
#define BASECOMMAND_H_

#include "Arduino.h"
#include "argumentprocessors/ArgumentProcessor.hpp"

#include <iterator>
#include <vector>
//#include <pnew.cpp>  // placement new implementation

namespace bubo {
namespace commanding {
namespace commands {

class BaseCommand {

public:
	BaseCommand(uint16_t numOfArgs);

	virtual ~BaseCommand();

	virtual bool processArgument(byte arg) = 0;

	virtual void execute() const = 0;

	const bool isComplete() const;

protected:
	uint16_t numOfArgs;

	uint_fast8_t processedArgs;

//	std::vector<ArgumentProcessor*> argumentProcessors;

};

} /* namespace commands */
} /* namespace commanding */
} /* namespace bubo */
#endif /* BASECOMMAND_H_ */
