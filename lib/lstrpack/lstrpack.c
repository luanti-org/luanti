// SPDX-License-Identifier: MIT
// PUC Lua string packing / unpacking library, with minor modifications for integration in Luanti.
// Extracted from https://github.com/lua/lua/blob/6e22fedb74cf0c9b6656e9fce8b7331db847c605/lstrlib.c

#include "lstrpack.h"

#include <stddef.h>
#include <string.h>
#include <limits.h>
#include <inttypes.h>
#include <stdlib.h>
#include <assert.h>

typedef uint64_t lua_Unsigned;

#define l_unlikely(x) (x)

/*
@@ LUAI_MAXALIGN defines fields that, when used in a union, ensure
** maximum alignment for the other items in that union.
*/
#define LUAI_MAXALIGN  lua_Number n; double u; void *s; lua_Integer i; long l

/*
** Some sizes are better limited to fit in 'int', but must also fit in
** 'size_t'. (We assume that 'lua_Integer' cannot be smaller than 'int'.)
*/
#define MAX_SIZET	((size_t)(~(size_t)0))

#define MAXSIZE  \
	(sizeof(size_t) < sizeof(int) ? MAX_SIZET : (size_t)(INT_MAX))

/*
** translate a relative initial string position
** (negative means back from end): clip result to [1, inf).
** The length of any string in Lua must fit in a lua_Integer,
** so there are no overflows in the casts.
** The inverted comparison avoids a possible overflow
** computing '-pos'.
*/
static size_t posrelatI (lua_Integer pos, size_t len) {
  if (pos > 0)
    return (size_t)pos;
  else if (pos == 0)
    return 1;
  else if (pos < -(lua_Integer)len)  /* inverted comparison */
    return 1;  /* clip to 1 */
  else return len + (size_t)pos + 1;
}

static_assert(LUAL_BUFFERSIZE >= 16, "LUAL_BUFFERSIZE too small");

/*
** {======================================================
** PACK/UNPACK
** =======================================================
*/


/* value used for padding */
#if !defined(LUAL_PACKPADBYTE)
#define LUAL_PACKPADBYTE		0x00
#endif

/* maximum size for the binary representation of an integer */
#define MAXINTSIZE	16

/* number of bits in a character */
#define NB	CHAR_BIT

/* mask for one character (NB 1's) */
#define MC	((1 << NB) - 1)

/* size of a lua_Integer */
#define SZINT	((int)sizeof(lua_Integer))


/* dummy union to get native endianness */
static const union {
  int dummy;
  char little;  /* true iff machine is little endian */
} nativeendian = {1};


/*
** information to pack/unpack stuff
*/
typedef struct Header {
  lua_State *L;
  int islittle;
  int maxalign;
} Header;


/*
** options for pack/unpack
*/
typedef enum KOption {
  Kint,		/* signed integers */
  Kuint,	/* unsigned integers */
  Kfloat,	/* single-precision floating-point numbers */
  Knumber,	/* Lua "native" floating-point numbers */
  Kdouble,	/* double-precision floating-point numbers */
  Kchar,	/* fixed-length strings */
  Kstring,	/* strings with prefixed length */
  Kzstr,	/* zero-terminated strings */
  Kpadding,	/* padding */
  Kpaddalign,	/* padding for alignment */
  Knop		/* no-op (configuration or spaces) */
} KOption;


/*
** Read an integer numeral from string 'fmt' or return 'df' if
** there is no numeral
*/
static int digit (int c) { return '0' <= c && c <= '9'; }

static int getnum (const char **fmt, int df) {
  if (!digit(**fmt))  /* no number? */
    return df;  /* return default value */
  else {
    int a = 0;
    do {
      a = a*10 + (*((*fmt)++) - '0');
    } while (digit(**fmt) && a <= ((int)MAXSIZE - 9)/10);
    return a;
  }
}


/*
** Read an integer numeral and raises an error if it is larger
** than the maximum size for integers.
*/
static int getnumlimit (Header *h, const char **fmt, int df) {
  int sz = getnum(fmt, df);
  if (l_unlikely(sz > MAXINTSIZE || sz <= 0))
    return luaL_error(h->L, "integral size (%d) out of limits [1,%d]",
                            sz, MAXINTSIZE);
  return sz;
}


/*
** Initialize Header
*/
static void initheader (lua_State *L, Header *h) {
  h->L = L;
  h->islittle = nativeendian.little;
  h->maxalign = 1;
}


/*
** Read and classify next option. 'size' is filled with option's size.
*/
static KOption getoption (Header *h, const char **fmt, int *size) {
  /* dummy structure to get native alignment requirements */
  struct cD { char c; union { LUAI_MAXALIGN; } u; };
  int opt = *((*fmt)++);
  *size = 0;  /* default */
  switch (opt) {
    case 'b': *size = sizeof(char); return Kint;
    case 'B': *size = sizeof(char); return Kuint;
    case 'h': *size = sizeof(short); return Kint;
    case 'H': *size = sizeof(short); return Kuint;
    case 'l': *size = sizeof(long); return Kint;
    case 'L': *size = sizeof(long); return Kuint;
    case 'j': *size = sizeof(lua_Integer); return Kint;
    case 'J': *size = sizeof(lua_Integer); return Kuint;
    case 'T': *size = sizeof(size_t); return Kuint;
    case 'f': *size = sizeof(float); return Kfloat;
    case 'n': *size = sizeof(lua_Number); return Knumber;
    case 'd': *size = sizeof(double); return Kdouble;
    case 'i': *size = getnumlimit(h, fmt, sizeof(int)); return Kint;
    case 'I': *size = getnumlimit(h, fmt, sizeof(int)); return Kuint;
    case 's': *size = getnumlimit(h, fmt, sizeof(size_t)); return Kstring;
    case 'c':
      *size = getnum(fmt, -1);
      if (l_unlikely(*size == -1))
        luaL_error(h->L, "missing size for format option 'c'");
      return Kchar;
    case 'z': return Kzstr;
    case 'x': *size = 1; return Kpadding;
    case 'X': return Kpaddalign;
    case ' ': break;
    case '<': h->islittle = 1; break;
    case '>': h->islittle = 0; break;
    case '=': h->islittle = nativeendian.little; break;
    case '!': {
      const int maxalign = offsetof(struct cD, u);
      h->maxalign = getnumlimit(h, fmt, maxalign);
      break;
    }
    default: luaL_error(h->L, "invalid format option '%c'", opt);
  }
  return Knop;
}


/*
** Read, classify, and fill other details about the next option.
** 'psize' is filled with option's size, 'notoalign' with its
** alignment requirements.
** Local variable 'size' gets the size to be aligned. (Kpadal option
** always gets its full alignment, other options are limited by
** the maximum alignment ('maxalign'). Kchar option needs no alignment
** despite its size.
*/
static KOption getdetails (Header *h, size_t totalsize,
                           const char **fmt, int *psize, int *ntoalign) {
  KOption opt = getoption(h, fmt, psize);
  int align = *psize;  /* usually, alignment follows size */
  if (opt == Kpaddalign) {  /* 'X' gets alignment from following option */
    if (**fmt == '\0' || getoption(h, fmt, &align) == Kchar || align == 0)
      luaL_argerror(h->L, 1, "invalid next option for option 'X'");
  }
  if (align <= 1 || opt == Kchar)  /* need no alignment? */
    *ntoalign = 0;
  else {
    if (align > h->maxalign)  /* enforce maximum alignment */
      align = h->maxalign;
    if (l_unlikely((align & (align - 1)) != 0))  /* not a power of 2? */
      luaL_argerror(h->L, 1, "format asks for alignment not power of 2");
    *ntoalign = (align - (int)(totalsize & (align - 1))) & (align - 1);
  }
  return opt;
}


/*
** Pack integer 'n' with 'size' bytes and 'islittle' endianness.
** The final 'if' handles the case when 'size' is larger than
** the size of a Lua integer, correcting the extra sign-extension
** bytes if necessary (by default they would be zeros).
*/
static void packint (lua_State *L, luaL_Buffer *b, lua_Unsigned n,
                     int islittle, int size, int neg) {
  if (l_unlikely(size > LUAL_BUFFERSIZE)) {
    // At least 16 is allowed, more isn't very useful
  	luaL_error(L, "Integer size too large: %d, max is %d", size, LUAL_BUFFERSIZE);
  	return;
  }
  char *buff = luaL_prepbuffer(b);
  int i;
  buff[islittle ? 0 : size - 1] = (char)(n & MC);  /* first byte */
  for (i = 1; i < size; i++) {
    n >>= NB;
    buff[islittle ? i : size - 1 - i] = (char)(n & MC);
  }
  if (neg && size > SZINT) {  /* negative number need sign extension? */
    for (i = SZINT; i < size; i++)  /* correct extra bytes */
      buff[islittle ? i : size - 1 - i] = (char)MC;
  }
  luaL_addsize(b, size);  /* add result to buffer */
}


/*
** Copy 'size' bytes from 'src' to 'dest', correcting endianness if
** given 'islittle' is different from native endianness.
*/
static void copywithendian (char *dest, const char *src,
                            int size, int islittle) {
  if (islittle == nativeendian.little)
    memcpy(dest, src, size);
  else {
    dest += size - 1;
    while (size-- != 0)
      *(dest--) = *(src++);
  }
}


static int str_pack (lua_State *L) {
  luaL_Buffer b;
  Header h;
  const char *fmt = luaL_checkstring(L, 1);  /* format string */
  int arg = 1;  /* current argument to pack */
  size_t totalsize = 0;  /* accumulate total size of result */
  initheader(L, &h);
  lua_pushnil(L);  /* mark to separate arguments from string buffer */
  luaL_buffinit(L, &b);
  while (*fmt != '\0') {
    int size, ntoalign;
    KOption opt = getdetails(&h, totalsize, &fmt, &size, &ntoalign);
    totalsize += ntoalign + size;
    while (ntoalign-- > 0)
     luaL_addchar(&b, LUAL_PACKPADBYTE);  /* fill alignment */
    arg++;
    switch (opt) {
      case Kint: {  /* signed integers */
        lua_Integer n = luaL_checkinteger(L, arg);
        if (size < SZINT) {  /* need overflow check? */
          lua_Integer lim = (lua_Integer)1 << ((size * NB) - 1);
          luaL_argcheck(L, -lim <= n && n < lim, arg, "integer overflow");
        }
        packint(L, &b, (lua_Unsigned)n, h.islittle, size, (n < 0));
        break;
      }
      case Kuint: {  /* unsigned integers */
        lua_Integer n = luaL_checkinteger(L, arg);
        if (size < SZINT)  /* need overflow check? */
          luaL_argcheck(L, (lua_Unsigned)n < ((lua_Unsigned)1 << (size * NB)),
                           arg, "unsigned overflow");
        packint(L, &b, (lua_Unsigned)n, h.islittle, size, 0);
        break;
      }
      case Kfloat: {  /* C float */
        float f = (float)luaL_checknumber(L, arg);  /* get argument */
        char *buff = luaL_prepbuffer(&b);
        /* move 'f' to final result, correcting endianness if needed */
        copywithendian(buff, (char *)&f, sizeof(f), h.islittle);
        luaL_addsize(&b, size);
        break;
      }
      case Knumber: {  /* Lua float */
        lua_Number f = luaL_checknumber(L, arg);  /* get argument */
        char *buff = luaL_prepbuffer(&b);
        /* move 'f' to final result, correcting endianness if needed */
        copywithendian(buff, (char *)&f, sizeof(f), h.islittle);
        luaL_addsize(&b, size);
        break;
      }
      case Kdouble: {  /* C double */
        double f = (double)luaL_checknumber(L, arg);  /* get argument */
        char *buff = luaL_prepbuffer(&b);
        /* move 'f' to final result, correcting endianness if needed */
        copywithendian(buff, (char *)&f, sizeof(f), h.islittle);
        luaL_addsize(&b, size);
        break;
      }
      case Kchar: {  /* fixed-size string */
        size_t len;
        const char *s = luaL_checklstring(L, arg, &len);
        luaL_argcheck(L, len <= (size_t)size, arg,
                         "string longer than given size");
        luaL_addlstring(&b, s, len);  /* add string */
        while (len++ < (size_t)size)  /* pad extra space */
          luaL_addchar(&b, LUAL_PACKPADBYTE);
        break;
      }
      case Kstring: {  /* strings with length count */
        size_t len;
        const char *s = luaL_checklstring(L, arg, &len);
        luaL_argcheck(L, size >= (int)sizeof(size_t) ||
                         len < ((size_t)1 << (size * NB)),
                         arg, "string length does not fit in given size");
        packint(L, &b, (lua_Unsigned)len, h.islittle, size, 0);  /* pack length */
        luaL_addlstring(&b, s, len);
        totalsize += len;
        break;
      }
      case Kzstr: {  /* zero-terminated string */
        size_t len;
        const char *s = luaL_checklstring(L, arg, &len);
        luaL_argcheck(L, strlen(s) == len, arg, "string contains zeros");
        luaL_addlstring(&b, s, len);
        luaL_addchar(&b, '\0');  /* add zero at the end */
        totalsize += len + 1;
        break;
      }
      case Kpadding: luaL_addchar(&b, LUAL_PACKPADBYTE);  /* FALLTHROUGH */
      case Kpaddalign: case Knop:
        arg--;  /* undo increment */
        break;
    }
  }
  luaL_pushresult(&b);
  return 1;
}


static int str_packsize (lua_State *L) {
  Header h;
  const char *fmt = luaL_checkstring(L, 1);  /* format string */
  size_t totalsize = 0;  /* accumulate total size of result */
  initheader(L, &h);
  while (*fmt != '\0') {
    int size, ntoalign;
    KOption opt = getdetails(&h, totalsize, &fmt, &size, &ntoalign);
    luaL_argcheck(L, opt != Kstring && opt != Kzstr, 1,
                     "variable-length format");
    size += ntoalign;  /* total space used by option */
    luaL_argcheck(L, totalsize <= MAXSIZE - size, 1,
                     "format result too large");
    totalsize += size;
  }
  lua_pushinteger(L, (lua_Integer)totalsize);
  return 1;
}


/*
** Unpack an integer with 'size' bytes and 'islittle' endianness.
** If size is smaller than the size of a Lua integer and integer
** is signed, must do sign extension (propagating the sign to the
** higher bits); if size is larger than the size of a Lua integer,
** it must check the unread bytes to see whether they do not cause an
** overflow.
*/
static lua_Integer unpackint (lua_State *L, const char *str,
                              int islittle, int size, int issigned) {
  lua_Unsigned res = 0;
  int i;
  int limit = (size  <= SZINT) ? size : SZINT;
  for (i = limit - 1; i >= 0; i--) {
    res <<= NB;
    res |= (lua_Unsigned)(unsigned char)str[islittle ? i : size - 1 - i];
  }
  if (size < SZINT) {  /* real size smaller than lua_Integer? */
    if (issigned) {  /* needs sign extension? */
      lua_Unsigned mask = (lua_Unsigned)1 << (size*NB - 1);
      res = ((res ^ mask) - mask);  /* do sign extension */
    }
  }
  else if (size > SZINT) {  /* must check unread bytes */
    int mask = (!issigned || (lua_Integer)res >= 0) ? 0 : MC;
    for (i = limit; i < size; i++) {
      if (l_unlikely((unsigned char)str[islittle ? i : size - 1 - i] != mask))
        luaL_error(L, "%d-byte integer does not fit into Lua Integer", size);
    }
  }
  return (lua_Integer)res;
}


static int str_unpack (lua_State *L) {
  Header h;
  const char *fmt = luaL_checkstring(L, 1);
  size_t ld;
  const char *data = luaL_checklstring(L, 2, &ld);
  size_t pos = posrelatI(luaL_optinteger(L, 3, 1), ld) - 1;
  int n = 0;  /* number of results */
  luaL_argcheck(L, pos <= ld, 3, "initial position out of string");
  initheader(L, &h);
  while (*fmt != '\0') {
    int size, ntoalign;
    KOption opt = getdetails(&h, pos, &fmt, &size, &ntoalign);
    luaL_argcheck(L, (size_t)ntoalign + size <= ld - pos, 2,
                    "data string too short");
    pos += ntoalign;  /* skip alignment */
    /* stack space for item + next position */
    luaL_checkstack(L, 2, "too many results");
    n++;
    switch (opt) {
      case Kint:
      case Kuint: {
        lua_Integer res = unpackint(L, data + pos, h.islittle, size,
                                       (opt == Kint));
        lua_pushinteger(L, res);
        break;
      }
      case Kfloat: {
        float f;
        copywithendian((char *)&f, data + pos, sizeof(f), h.islittle);
        lua_pushnumber(L, (lua_Number)f);
        break;
      }
      case Knumber: {
        lua_Number f;
        copywithendian((char *)&f, data + pos, sizeof(f), h.islittle);
        lua_pushnumber(L, f);
        break;
      }
      case Kdouble: {
        double f;
        copywithendian((char *)&f, data + pos, sizeof(f), h.islittle);
        lua_pushnumber(L, (lua_Number)f);
        break;
      }
      case Kchar: {
        lua_pushlstring(L, data + pos, size);
        break;
      }
      case Kstring: {
        size_t len = (size_t)unpackint(L, data + pos, h.islittle, size, 0);
        luaL_argcheck(L, len <= ld - pos - size, 2, "data string too short");
        lua_pushlstring(L, data + pos + size, len);
        pos += len;  /* skip string */
        break;
      }
      case Kzstr: {
        size_t len = strlen(data + pos);
        luaL_argcheck(L, pos + len < ld, 2,
                         "unfinished string for format 'z'");
        lua_pushlstring(L, data + pos, len);
        pos += len + 1;  /* skip string plus final '\0' */
        break;
      }
      case Kpaddalign: case Kpadding: case Knop:
        n--;  /* undo increment */
        break;
    }
    pos += size;
  }
  lua_pushinteger(L, pos + 1);  /* next position */
  return n + 1;
}

/* }====================================================== */

void setup_lstrpack(lua_State *L) {
	lua_getglobal(L, "string");
	int string = lua_gettop(L);
	lua_pushcfunction(L, str_pack);
	lua_setfield(L, string, "pack");
	lua_pushcfunction(L, str_unpack);
	lua_setfield(L, string, "unpack");
	lua_pushcfunction(L, str_packsize);
	lua_setfield(L, string, "packsize");
	lua_pop(L, 1);
}
