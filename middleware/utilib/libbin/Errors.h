/*******************************************************************************
*
*				Audio Framework
*				---------------
*
********************************************************************************
*	  Errors.h
********************************************************************************
*
*	  Description:	AudioWeaver Framework Error Codes
*
*	  Copyright:	DSP Concepts, Inc., 2007 - 2016
*					1800 Wyatt Drive, Suite 14
*					Sunnyvale, CA 95054
*
*******************************************************************************/

/**
 *	@addtogroup Errors
 *	@{
 */


#ifndef _ERRORS_H
#define _ERRORS_H



/** OK result */
#define E_SUCCESS (0)

/** Attempt to reference a non-existent heap. */
#define E_HEAP_INDEX_RANGE								(-1)

/** Attempt to allocate more storage with awe_fwMalloc() than exists. */
#define E_MALLOC_SIZE_TOO_BIG							(-2)

/** Attempt to allocate more storage with awe_fwMallocScratch() than exists. */
#define E_SCRATCH_ALLOC_SIZE_TOO_BIG					(-3)

/** A constructor was called with the wrong number of arguments. */
#define E_CONSTRUCTOR_ARGUMENT_COUNT					(-4)

/** Attempt to reference a non-existent class. */
#define E_CLASS_INDEX_RANGE								(-5)

/** Can't find the specified class. */
#define E_CLASS_NOT_FOUND								(-6)

/** Attempt to bind a module instance to a layout that is already owned by a layout. */
#define E_MODULE_ALREADY_OWNED							(-7)

/** Attempt to assign outside the bounds of the heap. */
#define E_ASSIGN_ADDRESS_OUT_OF_RANGE					(-8)

/** A wire input given to a module constructor is not a wire. */
#define E_MODULE_NOT_WIRE								(-9)

/** Many modules require the number of inputs and outputs to be the same. */
#define E_INPUTS_MUST_MATCH_OUTPUTS						(-10)

/** Many modules require that all inputs be of the same pintype. */
#define E_INPUTS_MUST_BE_SAME_PINTYPE					(-11)

/** Many modules require at leat one input. */
#define E_MUST_HAVE_INPUTS								(-12)

/** Many modules require at leat one output. */
#define E_MUST_HAVE_OUTPUTS								(-13)

/** Many modules require the input and output types to match in pairs. */
#define E_INPUTS_MUST_MATCH_CORRESPONDING_OUTPUTS		(-14)

/** The module given is not a module. */
#define E_NOT_MODULE									(-15)

/** Module with fixed inputs/outputs has wrong count. */
#define E_INPUT_OUTPUT_COUNT							(-16)

/** Module arguments have an error. */
#define E_PARAMETER_ERROR								(-17)

/** awe_fwGetFirstObject() or awe_fwGetNextObject() reached end of object chain. */
#define E_NO_MORE_OBJECTS								(-18)

/** Object pointer given to awe_fwGetNextObject() does not point to a module instance. */
#define E_NOT_OBJECT_POINTER							(-19)

/** Object pointer given to ClassInputWire_Constructor() is not an input pin. */
#define E_NOT_INPUT_PIN									(-20)

/** The I/O pin given to ClassXXWire_Constructor() is already in use. */
#define E_IOPIN_IN_USE									(-21)

/** The wire and I/O pin type are not compatible. */
#define E_PIN_TYPES_NOT_COMPATIBLE						(-22)

/** The wire and I/O pin sizes are not compatible. */
#define E_PIN_SIZES_NOT_COMPATIBLE						(-23)

/** Object pointer given to ClassOutpuWire_Constructor() is not an output pin. */
#define E_NOT_OUTPUT_PIN								(-24)

/** awe_fwGetFirstIO() or awe_fwGetNextIO() reached end of object chain. */
#define E_NO_MORE_IOPINS								(-25)

/** Master pump function found no layouts to pump. */
#define E_NO_LAYOUTS									(-26)

/** For modules with a hardwired single output (like RMS). */
#define E_MUST_HAVE_ONE_OUTPUT							(-27)

/** For modules that need an output with 1 single output value (like RMS). */
#define E_OUTPUT_MUST_BE_SINGLE_VALUE					(-28)

/** For containers, the contained module required sizes don't match. */
#define E_INCOMPATIBLE_BLOCK_SIZES						(-29)

/** A write index in a vector is out of range. */
#define E_WIRE_INDEX_RANGE								(-30)

/** There is no event handler for this module. */
#define E_NO_EVENT_HANDLER								(-31)

/** Audio_Start() called with the audio already running. */
#define E_AUDIO_ALREADY_STARTED							(-32)

/** Audio_Stop() called with the audio already stopped. */
#define E_AUDIO_ALREADY_STOPPED							(-33)

/** Framework communications failure. */
#define E_COMMUNICATIONS_ERROR							(-34)

/** Standalone tuning definitions */
#define E_SALT_OBJECT_NOTFOUND							(-35)

/** Salt field range error */
#define E_SALT_FIELD_RANGE								(-36)

/** Salt state range error	*/
#define E_SALT_STATE_RANGE								(-37)

/** Attempt to pump for RS232. */
#define E_NOT_IMPLEMENTED_IN_RS232						(-38)

/** Bad packet - invalid argument */
#define E_BADPACKET										(-39)

/** Filename NULL */
#define E_BADFILE										(-40)

/** Filename length > 23. */
#define E_FILENAMELENGTH								(-41)

/** Can't create file. */
#define E_CANTCREATE									(-42)

/** Can't open the specified file. */
#define E_CANTOPEN										(-43)

/** The specified file does not exist. */
#define E_NOSUCHFILE									(-44)

/** An error occurred accessing resource */
#define E_IOERROR										(-45)

/** Find First File must be called First */
#define E_FIND_FIRST_FILE_NOT_CALLED					(-46)

/** No more files found */
#define E_NO_MORE_FILES									(-47)

/** The filename is not valid */
#define E_BAD_FILENAME									(-48)

/** Cannot perform operation while a file is open */
#define E_FILE_ALREADY_OPEN								(-49)

/** File was not found */
#define E_FILE_NOT_FOUND								(-50)

/** File attribut byte is invalid */
#define E_ILLEGAL_FILE_ATTRIBUTE						(-51)

/** File already exists */
#define E_FILE_ALREADY_EXISTS							(-52)

/** There is no file open */
#define E_NO_OPEN_FILE									(-53)

/** Out of file system space */
#define E_OUT_OF_SPACE									(-54)

/** End of file */
#define E_END_OF_FILE									(-55)

/** Read flash memory failed */
#define E_ERROR_READING_FLASH_MEMORY					(-56)

/** Write flash memory failed */
#define E_ERROR_WRITING_FLASH_MEMORY					(-57)

/** Erase flash memory failed */
#define E_ERROR_ERASING_FLASH_MEMORY					(-58)

/** Command not implemented on this platform */
#define E_COMMAND_NOT_IMPLEMENTED						(-59)

/** Internal subsystem allocation failure */
#define E_INTERNAL_MODULE_ALLOCATION_FAILURE			(-60)

/** General hardware related failure */
#define E_HARDWARE_FAILURE								(-61)

/** Invalid register to read */
#define E_REGISTER_INVALID								(-62)

/** Register cannot be accessed at this time */
#define E_REGISTER_BUSY									(-63)

/** Register function not implemented */
#define E_REGISTER_NOT_IMPLEMENTED						(-64)

/** Trying to write a read-only register */
#define E_REGISTER_READ_ONLY							(-65)

/** Attempt to create heap failed. */
#define E_NO_HEAP_MEMORY								(-66)

/** Argument value invalid. */
#define E_ARGUMENT_ERROR								(-67)

/** Attempt to set duplicate ID with SetID. */
#define E_DUPLICATE_ID									(-68)

/** Attempt to use ID outside 1..32767 with SetID. */
#define E_ID_OUT_OF_RANGE								(-69)

/** Attempt to modify read-only object header. */
#define E_READ_ONLY										(-70)

/** Pointer to heap is NULL */
#define E_BAD_HEAP_POINTER								(-71)

/** Heaps already initialized */
#define E_HEAPS_ALREADY_INITIALIZED						(-72)

/** Heaps not yet initialized */
#define E_HEAPS_NOT_INITIALIZED							(-73)

/** CFramework exception (null pointer?). */
#define E_EXCEPTION										(-74)

/** Bad packet - message length is too long */
#define E_MESSAGE_LENGTH_TOO_LONG						(-75)

/** Bad packet - CRC error */
#define E_CRC_ERROR										(-76)

/** Bad packet - invalid command ID */
#define E_UNKNOWN_MESSAGE								(-77)

/** Message timed out (C6 only). */
#define E_MSG_TIMEOUT									(-78)

/** Object ID not found */
#define E_OBJECT_ID_NOT_FOUND							(-79)

/** I/O pin not found. */
#define E_PIN_ID_NOT_FOUND								(-80)

/** Not a valid object. */
#define E_NOT_OBJECT									(-81)

/** Member index is out of object bounds. */
#define E_BAD_MEMBER_INDEX								(-82)

/** Attempt to access member of wrong class type. */
#define E_CLASS_NOT_SUPPORTED							(-83)

/** Attempt to pump when awe_fwTick() was not called. */
#define E_PUMP_OVERRUN									(-84)

/** Target is not a V5 target - legacy. */
#define E_NOT_V5										(-85)

/** The framework does not exist. */
#define E_NO_FRAMEWORK									(-86)

/** The specified core does not exist. */
#define E_NO_CORE										(-87)

/** Too many wires bound to pin. */
#define E_IOPIN_TOO_MANY								(-88)

/** Attempt to bind a wire already bound. */
#define E_WIRE_ALREADY_BOUND							(-89)

/** Required wires not specified. */
#define E_WIRES_NOT_SPECIFIED							(-90)

/** AWE instance not created. */
#define E_NOT_CREATED									(-91)

/** AWE instance already created. */
#define E_ALREADY_CREATED								(-92)

/** Can't pump, audio not started. */
#define E_AUDIO_NOT_STARTED								(-93)

/** Module instance linked list is corrupted. */
#define E_LINKEDLIST_CORRUPT							(-94)

/** The file content is invalid. */
#define E_INVALID_FILE									(-95)

/** The module was not initialized. */
#define E_MODULE_NOT_INITIALIZED						(-96)

// In order to keep all the information about errors in one place the error description
// table is declared in this header. However, only one C file should actually instantiate
// this. All the other files that include this header should ignore.

#ifdef DEFINE_ERROR_STRINGS
/** Table of error descriptions */
static const char *s_error_strings[] =
{
	"success",											//#define E_SUCCESS (0)
	"no such heap",										//#define E_HEAP_INDEX_RANGE (-1)
	"heap allocation request too large",				//#define E_MALLOC_SIZE_TOO_BIG (-2)
	"scratch allocation request too large",				//#define E_SCRATCH_ALLOC_SIZE_TOO_BIG (-3)
	"constructor argument count wrong",					//#define E_CONSTRUCTOR_ARGUMENT_COUNT (-4)
	"no such class index",								//#define E_CLASS_INDEX_RANGE (-5)
	"class name not found",								//#define E_CLASS_NOT_FOUND (-6)
	"module already owned by layout",					//#define E_MODULE_ALREADY_OWNED (-7)
	"address out of range",								//#define E_ASSIGN_ADDRESS_OUT_OF_RANGE (-8)
	"module used, wire expected",						//#define E_MODULE_NOT_WIRE (-9)
	"input must match outputs",							//#define E_INPUTS_MUST_MATCH_OUTPUTS (-10)
	"input pintypes don't match",						//#define E_INPUTS_MUST_BE_SAME_PINTYPE (-11)
	"input(s) must be specified",						//#define E_MUST_HAVE_INPUTS (-12)
	"output(s) must be specified",						//#define E_MUST_HAVE_OUTPUTS (-13)
	"inputs must match corresponding outputs",			//#define E_INPUTS_MUST_MATCH_CORRESPONDING_OUTPUTS (-14)
	"not a module",										//#define E_NOT_MODULE (-15)
	"input/output count wrong",							//#define E_INPUT_OUTPUT_COUNT (-16)
	"Parameter error",									//#define E_PARAMETER_ERROR (-17)
	"no more objects found",							//#define E_NO_MORE_OBJECTS (-18)
	"pointer value is invalid",							//#define E_NOT_OBJECT_POINTER (-19)
	"not an input pin",									//#define E_NOT_INPUT_PIN (-20)
	"I/O pin is in use",								//#define E_IOPIN_IN_USE (-21)
	"pin types not compatible",							//#define E_PIN_TYPES_NOT_COMPATIBLE (-22)
	"pin sizes not compatible",							//#define E_PIN_SIZES_NOT_COMPATIBLE (-23)
	"not an output pin",								//#define E_NOT_OUTPUT_PIN (-24)
	"no more pins found",								//#define E_NO_MORE_IOPINS (-25)
	"no layout(s) to pump",								//#define E_NO_LAYOUTS (-26)
	"must have one output",								//#define E_MUST_HAVE_ONE_OUTPUT (-27)
	"output must be single value",						//#define E_OUTPUT_MUST_BE_SINGLE_VALUE (-28)
	"incompatible block sizes",							//#define E_INCOMPATIBLE_BLOCK_SIZES (-29)
	"wire index out of range",							//#define E_WIRE_INDEX_RANGE (-30)
	"module has no event handler",						//#define E_NO_EVENT_HANDLER (-31)
	"audio already started",							//#define E_AUDIO_ALREADY_STARTED (-32)
	"audio already stopped",							//#define E_AUDIO_ALREADY_STOPPED (-33)
	"communications error",								//#define E_COMMUNICATIONS_ERROR (-34)
	"SALT object not found",							//#define E_SALT_OBJECT_NOTFOUND (-35)
	"SALT field range error",							//#define E_SALT_FIELD_RANGE (-36)
	"SALT state range error",							//#define E_SALT_STATE_RANGE (-37)
	"not implemented",									//#define E_NOT_IMPLEMENTED_IN_RS232 (-38)
	"bad packet received",								//#define E_BADPACKET (-39)
	"invalid filename",									//#define E_BADFILE (-40)
	"filename too long",								//#define E_FILENAMELENGTH (-41)
	"file create failed",								//#define E_CANTCREATE (-42)
	"file open failed",									//#define E_CANTOPEN (-43)
	"no such file",										//#define E_NOSUCHFILE (-44)
	"file I/O error",									//#define E_IOERROR (-45)
	"FindNext called without FindFirst",				//#define E_FIND_FIRST_FILE_NOT_CALLED (-46)
	"no more files found",								//#define E_NO_MORE_FILES (-47)
	"bad filename",										//#define E_BAD_FILENAME (-48)
	"file already open",								//#define E_FILE_ALREADY_OPEN (-49)
	"no such file",										//#define E_FILE_NOT_FOUND (-50)
	"illegal file attribute",							//#define E_ILLEGAL_FILE_ATTRIBUTE (-51)
	"file already exists",								//#define E_FILE_ALREADY_EXISTS (-52)
	"no open file",										//#define E_NO_OPEN_FILE (-53)
	"file system full",									//#define E_OUT_OF_SPACE (-54)
	"end of file",										//#define E_END_OF_FILE (-55)
	"FLASH read error",									//#define E_ERROR_READING_FLASH_MEMORY (-56)
	"FLASH write error",								//#define E_ERROR_WRITING_FLASH_MEMORY (-57)
	"FLASH erase error",								//#define E_ERROR_ERASING_FLASH_MEMORY (-58)
	"no such command",									//#define E_COMMAND_NOT_IMPLEMENTED (-59)
	"internal module allocation error",					//#define E_INTERNAL_MODULE_ALLOCATION_FAILURE (-60)
	"hardware failure",									//#define E_HARDWARE_FAILURE (-61)
	"register invalid",									//#define E_REGISTER_INVALID (-62)
	"register busy",									//#define E_REGISTER_BUSY (-63)
	"register not implemented",							//#define E_REGISTER_NOT_IMPLEMENTED (-64)
	"register read only",								//#define E_REGISTER_READ_ONLY (-65)
	"create heap failed",								//#define E_NO_HEAP_MEMORY (-66)
	"invalid argument",									//#define E_ARGUMENT_ERROR (-67)
	"duplicate ID",										//#define E_DUPLICATE_ID (-68)
	"ID out of range",									//#define E_ID_OUT_OF_RANGE (-69)
	"read only",										//#define E_READ_ONLY (-70)
	"bad heap pointer",									//#define E_BAD_HEAP_POINTER (-71)
	"heaps already initialized",						//#define E_HEAPS_ALREADY_INITIALIZED (-72)
	"heaps not initialized",							//#define E_HEAPS_NOT_INITIALIZED (-73)
	"framework exception",								//#define E_EXCEPTION (-74)
	"message too long",									//#define E_MESSAGE_LENGTH_TOO_LONG (-75)
	"CRC error",										//#define E_CRC_ERROR (-76)
	"unimplemented or invalid command ID",				//#define E_UNKNOWN_MESSAGE (-77)
	"message timed out",								//#define E_MSG_TIMEOUT (-78)
	"object ID not found",								//#define E_OBJECT_ID_NOT_FOUND (-79)
	"no such pin",										//#define E_PIN_ID_NOT_FOUND (-80)
	"not an object",									//#define E_NOT_OBJECT (-81)
	"bad member index",									//#define E_BAD_MEMBER_INDEX (-82)
	"not supported",									//#define E_CLASS_NOT_SUPPORTED )-83)
	"audio overrun",									//#define E_PUMP_OVERRUN (-84)
	"not a V4 target",									//#define E_NOT_V5 (-85)
	"no framework",										//#define E_NO_FRAMEWORK (-86)
	"no such core",										//#define E_NO_CORE (-87)
	"too many bound wires",								//#define E_IOPIN_TOO_MANY (-88)
	"wire already bound",								//#define E_WIRE_ALREADY_BOUND (-89)
	"wires must be specified",							//#define E_WIRES_NOT_SPECIFIED (-90)
	"instance not created",								//#define E_NOT_CREATED (-91)
	"instance already created",							//#define E_ALREADY_CREATED (-92)
	"audio not started",								//#define E_AUDIO_NOT_STARTED (-93)
	"linked list corrupted",							//#define E_LINKEDLIST_CORRUPT (-94)
	"invalid file content",								//#define E_INVALID_FILE (-95)
	"module not initialized",							//#define E_MODULE_NOT_INITIALIZED (-96)
};
#endif

#endif	// _ERRORS_H

/**
 * @}
 *
 * End of file.
 */
