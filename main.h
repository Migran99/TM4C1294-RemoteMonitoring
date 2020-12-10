#ifndef __ENET_WEATER_H__
#define __ENET_WEATER_H__

#include <stdint.h>
#include <stdbool.h>

//*****************************************************************************
//
// If building with a C++ compiler, make all of the definitions in this header
// have a C binding.
//
//*****************************************************************************
#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
// Defines for setting up the system tick clock.
//
//*****************************************************************************
#define SYSTEM_TICK_MS          10
#define SYSTEM_TICK_S           100


//*****************************************************************************
//
// Input command line buffer size.
//
//*****************************************************************************
#define APP_INPUT_BUF_SIZE                  1024

#define MAX_REQUEST_SIZE                    500


//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif

#endif // __ENET_WEATER_H__
