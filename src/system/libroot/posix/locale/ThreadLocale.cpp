/*
 * Copyright 2022, Trung Nguyen, trungnt282910@gmail.com
 * All rights reserved. Distributed under the terms of the MIT License.
 */


#include <ctype.h>

#include <tls.h>
#include <kernel/OS.h>
#include <ThreadLocale.h>



namespace BPrivate {
namespace Libroot {


static void
DestroyThreadLocale(void* ptr)
{
	ThreadLocale* threadLocale = (ThreadLocale*)ptr;
	delete threadLocale;
}


ThreadLocale*
GetCurrentThreadLocale()
{
	ThreadLocale* threadLocale = (ThreadLocale*)tls_get(TLS_LOCALE_SLOT);
	if (threadLocale == NULL) {
		threadLocale = new ThreadLocale();
		threadLocale->threadLocaleInfo = NULL;
		threadLocale->ctype_b = __ctype_b;
		threadLocale->ctype_tolower = __ctype_tolower;
		threadLocale->ctype_toupper = __ctype_toupper;

		on_exit_thread(DestroyThreadLocale, threadLocale);
		tls_set(TLS_LOCALE_SLOT, threadLocale);
	}
	return threadLocale;
}


extern "C" const unsigned short**
__ctype_b_loc()
{
	return &GetCurrentThreadLocale()->ctype_b;
}


extern "C" const int**
__ctype_tolower_loc()
{
	return &GetCurrentThreadLocale()->ctype_tolower;
}


extern "C" const int**
__ctype_toupper_loc()
{
	return &GetCurrentThreadLocale()->ctype_toupper;
}


}	// namespace Libroot
}	// namespace BPrivate
