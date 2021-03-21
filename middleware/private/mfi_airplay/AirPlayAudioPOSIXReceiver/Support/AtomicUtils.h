/*
	File:    AtomicUtils.h
	Package: AirPlayAudioPOSIXReceiver
	Version: AirPlay_Audio_POSIX_Receiver_211.1.p8
	
	Disclaimer: IMPORTANT: This Apple software is supplied to you, by Apple Inc. ("Apple"), in your
	capacity as a current, and in good standing, Licensee in the MFi Licensing Program. Use of this
	Apple software is governed by and subject to the terms and conditions of your MFi License,
	including, but not limited to, the restrictions specified in the provision entitled ”Public
	Software”, and is further subject to your agreement to the following additional terms, and your
	agreement that the use, installation, modification or redistribution of this Apple software
	constitutes acceptance of these additional terms. If you do not agree with these additional terms,
	please do not use, install, modify or redistribute this Apple software.
	
	Subject to all of these terms and in?consideration of your agreement to abide by them, Apple grants
	you, for as long as you are a current and in good-standing MFi Licensee, a personal, non-exclusive
	license, under Apple's copyrights in this original Apple software (the "Apple Software"), to use,
	reproduce, and modify the Apple Software in source form, and to use, reproduce, modify, and
	redistribute the Apple Software, with or without modifications, in binary form. While you may not
	redistribute the Apple Software in source form, should you redistribute the Apple Software in binary
	form, you must retain this notice and the following text and disclaimers in all such redistributions
	of the Apple Software. Neither the name, trademarks, service marks, or logos of Apple Inc. may be
	used to endorse or promote products derived from the Apple Software without specific prior written
	permission from Apple. Except as expressly stated in this notice, no other rights or licenses,
	express or implied, are granted by Apple herein, including but not limited to any patent rights that
	may be infringed by your derivative works or by other works in which the Apple Software may be
	incorporated.
	
	Unless you explicitly state otherwise, if you provide any ideas, suggestions, recommendations, bug
	fixes or enhancements to Apple in connection with this software (“Feedback”), you hereby grant to
	Apple a non-exclusive, fully paid-up, perpetual, irrevocable, worldwide license to make, use,
	reproduce, incorporate, modify, display, perform, sell, make or have made derivative works of,
	distribute (directly or indirectly) and sublicense, such Feedback in connection with Apple products
	and services. Providing this Feedback is voluntary, but if you do provide Feedback to Apple, you
	acknowledge and agree that Apple may exercise the license granted above without the payment of
	royalties or further consideration to Participant.
	
	The Apple Software is provided by Apple on an "AS IS" basis. APPLE MAKES NO WARRANTIES, EXPRESS OR
	IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY
	AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR
	IN COMBINATION WITH YOUR PRODUCTS.
	
	IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL DAMAGES
	(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
	PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION
	AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
	(INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
	
	Copyright (C) 2001-2014 Apple Inc. All Rights Reserved.
*/
/*!
    @header		Atomic Operations 
    @discussion	Provides lock-free atomic operations like such as incrementing an integer, compare-and-swap, etc.
*/
/*!
    @header Atomic Operations 
    @discussion Provides lock-free atomic operations like such as incrementing an integer, compare-and-swap, etc.
*/

#ifndef	__AtomicUtils_h__
#define	__AtomicUtils_h__

#if( defined( AtomicUtils_PLATFORM_HEADER ) )
	#include  AtomicUtils_PLATFORM_HEADER
#endif

#include "APSCommonServices.h"

#ifdef __cplusplus
extern "C" {
#endif

#if 0
//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		Atomic Operations
	@abstract	Provides lock-free atomic operations like such as incrementing an integer, compare-and-swap, etc.

    @discussion 

	AtomicUtils provides lock-free atomic operations, such as incrementing an integer, compare-and-swap, etc. For many platforms,
	this porting has already been done either by implementing hardware/OS-specific code for it or by relying on features of the
	compiler to do it (e.g. __sync* builtins from clang/GCC, RAS on NetBSD). If porting is required, the following functions need to
	be implemented in an atomic, and ideally lock-free, manner. 

	NOTE: GCC based implementation of these functions are provided.
*/

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	atomic_add_and_fetch_32

    @discussion 

	This function atomically adds the value of inVal to the variable that inPtr points to. The result is stored in the address that
	is specified by inPtr. 

        i.e. { *inPtr += inVal; return *inPtr; }

	NOTE: GCC based implementation of this function is provided. If porting is required, the following functions need to
	be implemented in an atomic, and ideally lock-free, manner. 
*/
int32_t atomic_add_and_fetch_32( int32_t *inPtr, int32_t inVal );


//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	atomic_bool_compare_and_swap_32

    @discussion 

	This function compares the value of inOldValue with the value of the variable that inPtr points to. If they are equal, the value
	of inNewValue is stored in the address that is specified by inPtr; otherwise, no operation is performed. If the value of
	inOldValue and the value of the variable that inPtr points to are equal, the function returns true, otherwise it returns false.

        i.e.  { if( *inPtr == inOldValue ) { *inPtr = inNewValue; return 1; } else return 0; }

	NOTE: GCC based implementation of this function is provided. If porting is required, the following functions need to
	be implemented in an atomic, and ideally lock-free, manner. 
*/
Boolean atomic_bool_compare_and_swap_32( int32_t *inPtr, int32_t inOldValue, int32_t inNewValue );


//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	atomic_fetch_and_and_32

    @discussion 

	This function performs an atomic bitwise OR operation on the variable inVal with the variable that inPtr points to. The result
	is stored in the address that is specified by inPtr. The function returns the initial value of the variable that inPtr points to.

        i.e. { tmp = *inPtr; *inPtr &= inVal; return tmp; }

	NOTE: GCC based implementation of this function is provided. If porting is required, the following functions need to
	be implemented in an atomic, and ideally lock-free, manner. 
*/
int32_t atomic_fetch_and_and_32( int32_t *inPtr, int32_t inVal );


//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	atomic_fetch_and_or_32

    @discussion 

	This function atomically adds the value of inVal to the variable that inPtr points to. The result is stored in the address that
	is specified by inPtr. The function returns the initial value of the variable that inPtr points to.

        i.e. { tmp = *inPtr; *inPtr |= inVal; return tmp; }

	NOTE: GCC based implementation of this function is provided. If porting is required, the following functions need to
	be implemented in an atomic, and ideally lock-free, manner. 
*/
int32_t atomic_fetch_and_or_32( int32_t *inPtr, int32_t inVal );


//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	atomic_read_write_barrier

    @discussion 

	This function ensures reads and writes before this line have completed.

*/
void atomic_read_write_barrier( void );


//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	atomic_sub_and_fetch_32

    @discussion 

	This function atomically subtracts the value of inVal from the variable that inPtr points to. The result is stored in the
	address that is specified by inPtr. The function returns the new value of the variable that inPtr points to.

        i.e. { *inPtr -= inVal; return *inPtr; }

	NOTE: GCC based implementation of this function is provided. If porting is required, the following functions need to
	be implemented in an atomic, and ideally lock-free, manner. 
*/
int32_t atomic_sub_and_fetch_32( int32_t *inPtr, int32_t inVal );


//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	atomic_val_compare_and_swap_32

    @discussion 

	This function compares the value of inOldValue to the value of the variable that inPtr points to. If they are equal, the value
	of inNewValue is stored in the address that is specified by inPtr, otherwise, no operation is performed. The function returns
	the initial value of the variable that inPtr points to.

        i.e. { if( *inPtr == inOldValue ) { *inPtr = inNewValue; return inOldValue; } else return *inPtr; }

	NOTE: GCC based implementation of this function is provided. If porting is required, the following functions need to
	be implemented in an atomic, and ideally lock-free, manner. 
*/
int32_t atomic_val_compare_and_swap_32( int32_t *inPtr, int32_t inOldValue, int32_t inNewValue );


//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	atomic_yield

    @discussion 

	This function yields the current thread to allow other threads to execute.
*/
void atomic_yield( void );


// clang and newer versions of GCC provide atomic builtins. GCC introduced these in 4.2, but for some targets, 
// such as ARM, they didn't show up until later (4.5 for ARM tested, but it's possible earlier versions also 
// have them). It's also possible that specific targets may not have these builtins even with newer versions
// of GCC. There isn't a good way to detect support for it this assumes it works and you can override if not.

#if( !defined( AtomicUtils_HAS_SYNC_BUILTINS ) )
	#if( __clang__ || ( ( __GNUC__ > 4 ) || ( ( __GNUC__ == 4 ) && ( __GNUC_MINOR__ >= 5 ) ) ) )
		#define AtomicUtils_HAS_SYNC_BUILTINS		1
	#else
		#define AtomicUtils_HAS_SYNC_BUILTINS		0
	#endif
#endif

#if( AtomicUtils_USE_PTHREADS )

#if 0
#pragma mark == pthreads ==
#endif

//===========================================================================================================================
//	pthreads
//===========================================================================================================================

int32_t	atomic_add_and_fetch_32( int32_t *inPtr, int32_t inVal );
int32_t	atomic_fetch_and_add_32( int32_t *inPtr, int32_t inVal );
int32_t	atomic_fetch_and_store_32( int32_t *inPtr, int32_t inVal );
#endif
#elif( AtomicUtils_HAS_SYNC_BUILTINS )

#if 0
#pragma mark -
#pragma mark == __sync builtins ==
#endif

//===========================================================================================================================
//	Sync Builtins
//===========================================================================================================================
	
#define AtomicUtils_USE_BUILTINS		1

#define atomic_fetch_and_add_32( PTR, VAL )							__sync_fetch_and_add( (PTR), (VAL) )
#define atomic_fetch_and_sub_32( PTR, VAL )							__sync_fetch_and_sub( (PTR), (VAL) )
#define atomic_fetch_and_or_32( PTR, VAL )							__sync_fetch_and_or( (PTR), (VAL) )
#define atomic_fetch_and_and_32( PTR, VAL )							__sync_fetch_and_and( (PTR), (VAL) )
#define atomic_fetch_and_xor_32( PTR, VAL )							__sync_fetch_and_xor( (PTR), (VAL) )

#define atomic_add_and_fetch_32( PTR, VAL )							__sync_add_and_fetch( (PTR), (VAL) )
#define atomic_sub_and_fetch_32( PTR, VAL )							__sync_sub_and_fetch( (PTR), (VAL) )
#define atomic_or_and_fetch_32( PTR, VAL )							__sync_or_and_fetch( (PTR), (VAL) )
#define atomic_and_and_fetch_32( PTR, VAL )							__sync_and_and_fetch( (PTR), (VAL) )
#define atomic_xor_and_fetch_32( PTR, VAL )							__sync_xor_and_fetch( (PTR), (VAL) )

#define atomic_fetch_and_store_32( PTR, VAL )						__sync_lock_test_and_set( (PTR), (VAL) )
#define atomic_bool_compare_and_swap_32( PTR, OLD_VAL, NEW_VAL )	__sync_bool_compare_and_swap( (PTR), (OLD_VAL), (NEW_VAL) )
#define atomic_val_compare_and_swap_32( PTR, OLD_VAL, NEW_VAL )		__sync_val_compare_and_swap( (PTR), (OLD_VAL), (NEW_VAL) )

#if( defined( __i386__ ) || defined( __x86_64__ ) ) // GCC emits nothing for __sync_synchronize() on i386/x86_64.
	#define atomic_read_barrier()									__asm__ __volatile__( "mfence" )
	#define atomic_write_barrier()									__asm__ __volatile__( "mfence" )
	#define atomic_read_write_barrier()								__asm__ __volatile__( "mfence" )
#else
	#define atomic_read_barrier()									__sync_synchronize()
	#define atomic_write_barrier()									__sync_synchronize()
	#define atomic_read_write_barrier()								__sync_synchronize()
#endif

#elif( TARGET_CPU_MIPS )
//#include <atomic.h>
//#error "TARGET_CPU_MIPS"
#define atomic_inc(x) __sync_add_and_fetch((x),1)  
#define atomic_dec(x) __sync_sub_and_fetch((x),1)  
#define atomic_add(x,y) __sync_add_and_fetch((x),(y))  
#define atomic_sub(x,y) __sync_sub_and_fetch((x),(y))  
#define atomic_compare_and_exchange_val_acq(x,y,z)  __sync_val_compare_and_swap((x),(y),(z))
#define atomic_compare_and_exchange_bool_acq(x,y,z)  __sync_bool_compare_and_swap((x),(y),(z))
#if 0
#pragma mark -
#pragma mark == MIPS ==
#endif

//===========================================================================================================================
//	MIPS
//===========================================================================================================================

STATIC_INLINE int32_t					atomic_fetch_and_add_32( int32_t *inPtr, int32_t inVal )
{
	int old = *inPtr;
	*inPtr += inVal;
	return old;
}

STATIC_INLINE int32_t	atomic_fetch_and_sub_32( int32_t *inPtr, int32_t inVal )
{
	int old = *inPtr;
	*inPtr -= inVal;
	return old;
}

STATIC_INLINE int32_t					atomic_fetch_and_or_32(  int32_t *inPtr, int32_t inVal )
{
	int old = *inPtr;
	*inPtr |= inVal;
	return old;
}
STATIC_INLINE int32_t					atomic_fetch_and_and_32( int32_t *inPtr, int32_t inVal )
{
	int old = *inPtr;
	*inPtr &= inVal;
	return old;
}
STATIC_INLINE int32_t					atomic_fetch_and_xor_32( int32_t *inPtr, int32_t inVal )
{
	int old = *inPtr;
	*inPtr ^= inVal;
	return old;
}

STATIC_INLINE int32_t	atomic_add_and_fetch_32( int32_t *inPtr, int32_t inVal )
{
	atomic_add(inPtr, inVal);
	return *inPtr; 
}

STATIC_INLINE int32_t	atomic_sub_and_fetch_32( int32_t *inPtr, int32_t inVal )
{
	atomic_add(inPtr, -inVal);
	return *inPtr; 
}

STATIC_INLINE int32_t	atomic_or_and_fetch_32(  int32_t *inPtr, int32_t inVal )
{
	*inPtr |= inVal;
	return *inPtr;
}

STATIC_INLINE  int32_t	atomic_and_and_fetch_32( int32_t *inPtr, int32_t inVal )
{
	*inPtr &= inVal;
	return *inPtr;
}

STATIC_INLINE  int32_t	atomic_xor_and_fetch_32( int32_t *inPtr, int32_t inVal )
{ 
	*inPtr ^= inVal;
	return *inPtr;
}

STATIC_INLINE int32_t	atomic_fetch_and_store_32( int32_t *inPtr, int32_t inVal )
{
	int old = *inPtr;
	*inPtr = inVal;
	return old;
}

STATIC_INLINE int32_t		atomic_val_compare_and_swap_32( int32_t *inPtr, int32_t inOldValue, int32_t inNewValue )
{
	return atomic_compare_and_exchange_val_acq(inPtr, inOldValue, inNewValue);

}

STATIC_INLINE Boolean	atomic_bool_compare_and_swap_32( int32_t *inPtr, int32_t inOldValue, int32_t inNewValue )
{
	return atomic_compare_and_exchange_bool_acq(inPtr, inOldValue, inNewValue);
}

//#define atomic_read_barrier()			__asm__ __volatile__( "" : : : "memory" )
//#define atomic_write_barrier()			__asm__ __volatile__( "" : : : "memory" )
#define atomic_read_write_barrier()		__asm__ __volatile__( "" : : : "memory" )
#if 0
atomic_add_and_fetch_32
atomic_val_compare_and_swap_32
atomic_fetch_and_add_32
atomic_fetch_and_or_32
atomic_fetch_and_and_32
#endif
#endif

#if 0
#pragma mark -
#pragma mark == Generic ==
#endif

//===========================================================================================================================
//	Generic
//===========================================================================================================================

// Atomic APIs that don't return the result.

#if( !defined( atomic_add_32 ) )
	#define atomic_add_32( PTR, VAL )		( (void) atomic_add_and_fetch_32( (PTR), (VAL) ) )
#endif

#if( !defined( atomic_sub_32 ) )
	#define atomic_sub_32( PTR, VAL )		( (void) atomic_sub_and_fetch_32( (PTR), (VAL) ) )
#endif

#if( !defined( atomic_or_32 ) )
	#define atomic_or_32( PTR, VAL )		( (void) atomic_fetch_and_or_32(  (PTR), (VAL) ) )
#endif

#if( !defined( atomic_and_32 ) )
	#define atomic_and_32( PTR, VAL )		( (void) atomic_fetch_and_and_32( (PTR), (VAL) ) )
#endif

#if( !defined( atomic_xor_32 ) )
	#define atomic_xor_32( PTR, VAL )		( (void) atomic_fetch_and_xor_32( (PTR), (VAL) ) )
#endif

// atomic_yield -- Yield via software/thread.

	#define atomic_yield()		usleep( 1 )

// atomic_hardware_yield -- Yield via hardware. WARNING: may not yield if on a higher priority thread.

#if( TARGET_CPU_X86 || TARGET_CPU_X86_64 )
	#if( __GNUC__ )
		#define atomic_hardware_yield()		__asm__ __volatile__( "pause" )
	#endif
#elif( __ARMCC_VERSION ) // RealView ARM compiler (__yield() doesn't seem to work).
	#define atomic_hardware_yield()			__nop()
#elif( __GNUC__ )
	#define atomic_hardware_yield()			__asm__ __volatile__( "" )
#endif

// atomic_once

typedef int32_t	atomic_once_t;
typedef void ( *atomic_once_function_t )( void *inContext );

void	atomic_once_slow( atomic_once_t *inOnce, void *inContext, atomic_once_function_t inFunction );

#define atomic_once( ONCE, CONTEXT, FUNCTION ) \
	do \
	{ \
		if( __builtin_expect( *(ONCE), 2 ) != 2 ) \
		{ \
			atomic_once_slow( (ONCE), (CONTEXT), (FUNCTION) ); \
		} \
		\
	}	while( 0 )

// atomic_spinlock

typedef int32_t		atomic_spinlock_t;

#define atomic_spinlock_lock( SPIN_LOCK_PTR ) \
	do { while( !atomic_bool_compare_and_swap_32( (SPIN_LOCK_PTR), 0, 1 ) ) atomic_yield(); } while( 0 )

#define atomic_spinlock_unlock( SPIN_LOCK_PTR ) \
	do { atomic_read_write_barrier(); *(SPIN_LOCK_PTR) = 0; } while( 0 )

#if 0
#pragma mark -
#pragma mark == Debugging ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AtomicUtils_Test
	@internal
	@abstract	Unit test.
*/

OSStatus	AtomicUtils_Test( void );

#ifdef __cplusplus
}
#endif

#endif // __AtomicUtils_h__
