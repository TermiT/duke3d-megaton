#include <stdio.h>
#include <string.h>

extern "C"
size_t __strlcat_chk( char *dest, const char *src, size_t size, size_t destSize ) {
	return strlcat( dest, src, destSize );
}

extern "C"
size_t __strlcpy_chk( char *dest, const char *src, size_t size, size_t destSize ) {
    return strlcpy( dest, src, size );
}
