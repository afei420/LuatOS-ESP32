/*
@module  zbuff
@summary c内存数据操作库
@version 0.1
@date    2021.03.31
*/
#include "luat_base.h"
#include "luat_zbuff.h"
#include "luat_malloc.h"

#define LUAT_LOG_TAG "luat.zbuff"
#include "luat_log.h"

#define LUAT_ZBUFF_TYPE "ZBUFF*"

#define tozbuff(L) ((luat_zbuff *)luaL_checkudata(L, 1, LUAT_ZBUFF_TYPE))

//在buff对象后添加数据，返回增加的字节数
int add_bytes(luat_zbuff* buff,const char* source,size_t len)
{
    if(buff->len - buff->cursor < len)
        len = buff->len - buff->cursor;
    memcpy(buff->addr+buff->cursor,source,len);
    buff->cursor += len;
    return len;
}

/**
创建zbuff
@api zbuff.create(length,data)
@int 字节数
@any 可选参数，number时为填充数据，string时为填充字符串
@return object zbuff对象，如果创建失败会返回nil
@usage
-- 创建zbuff
local buff = zbuff.create(1024) -- 空白的
local buff = zbuff.create(1024, 0x33) --创建一个初值全为0x33的内存区域
local buff = zbuff.create(1024, "123321456654") -- 创建，并填充一个已有字符串的内容
 */
static int l_zbuff_create(lua_State *L)
{
    size_t len = luaL_checkinteger(L, 1);
    if (len <= 0)
    {
        return 0;
    }

    luat_zbuff *buff = (luat_zbuff *)lua_newuserdata(L, sizeof(luat_zbuff));
    if (buff == NULL)
    {
        return 0;
    }
    buff->addr = (uint8_t *)luat_heap_malloc(len);
    if (buff->addr == NULL)
    {
        lua_pushnil(L);
        lua_pushstring(L,"memory not enough");
        return 2;
    }

    buff->len = len;
    buff->cursor = 0;

    if(lua_isinteger(L,2)){
        memset(buff->addr, luaL_checkinteger(L,2) % 0x100, len);
    }
    else if (lua_isstring(L, 2))
    {
        const char *data = luaL_optlstring(L, 2, "", &len);
        if(len > buff->len)//防止越界
        {
            len = buff->len;
        }
        memcpy(buff->addr, data, len);
        buff->cursor = len;
    }
    else{
        memset(buff->addr, 0, len);
    }

    luaL_setmetatable(L, LUAT_ZBUFF_TYPE);
    return 1;
}

/**
zbuff写数据
@api buff:write(para,...)
@any 写入buff的数据，string时为一个参数，number时可为多个参数
@return number 数据成功写入的长度
@usage
-- 类file的读写操作
local len = buff:write("123") -- 写入数据, 指针相应地往后移动，返回写入的数据长度
local len = buff:write(0x1a,0x30,0x31,0x32,0x00,0x01)  -- 按数值写入多个字节数据
 */
static int l_zbuff_write(lua_State *L)
{
    if(lua_isinteger(L,2)){
        int len = 0,data;
        luat_zbuff *buff = tozbuff(L);
        while(lua_isinteger(L,2+len) && buff->cursor < buff->len){
            data = luaL_checkinteger(L,2+len);
            *(uint8_t*)(buff->addr+buff->cursor) = data % 0x100;
            buff->cursor++;
            len++;
        }
        lua_pushinteger(L, len);
        return 1;
    }
    else{
        size_t len;
        const char *data = luaL_checklstring(L, 2, &len);
        luat_zbuff *buff = tozbuff(L);
        if(len + buff->cursor > buff->len)//防止越界
        {
            len = buff->len - buff->cursor;
        }
        memcpy(buff->addr+buff->cursor, data, len);
        buff->cursor = buff->cursor + len;
        lua_pushinteger(L, len);
        return 1;
    }
}

/**
zbuff读数据
@api buff:read(length)
@int 读取buff中的字节数
@return string 读取结果
@usage
-- 类file的读写操作
local str = buff:read(3)
 */
static int l_zbuff_read(lua_State *L)
{
    luat_zbuff *buff = tozbuff(L);
    int read_num = luaL_optinteger(L, 2, 1);
    if(read_num > buff->len - buff->cursor)//防止越界
    {
        read_num = buff->len - buff->cursor;
    }
    if(read_num <= 0)
    {
        lua_pushlstring(L, NULL, 0);
        return 1;
    }
    char *return_str = (char *)luat_heap_malloc(read_num);
    if(return_str == NULL)
    {
        return 0;
    }
    memcpy(return_str, buff->addr+buff->cursor, read_num);
    lua_pushlstring(L, return_str, read_num);
    buff->cursor += read_num;
    return 1;
}

/**
zbuff设置光标位置
@api buff:seek(base,offset)
@int 偏移长度
@int whence, 基点，默认zbuff.SEEK_SET。zbuff.SEEK_SET: 基点为 0 （文件开头），zbuff.SEEK_CUR: 基点为当前位置，zbuff.SEEK_END: 基点为文件尾
@return int 设置光标后从buff开头计算起的光标的位置
@usage
buff:seek(0) -- 把光标设置到指定位置
buff:seek(5,zbuff.SEEK_CUR)
buff:seek(-3,zbuff.SEEK_END)
 */
static int l_zbuff_seek(lua_State *L)
{
    luat_zbuff *buff = tozbuff(L);

    int offset = luaL_checkinteger(L, 2);
    int whence = luaL_optinteger(L,3,ZBUFF_SEEK_SET);
    switch (whence)
    {
    case ZBUFF_SEEK_SET:
        break;
    case ZBUFF_SEEK_CUR:
        offset = buff->cursor + offset;
        break;
    case ZBUFF_SEEK_END:
        offset = buff->len + offset;
        break;
    default:
        return 0;
    }
    if(offset <= 0) offset = 0;
    if(offset > buff->len) offset = buff->len;
    buff->cursor = offset;
    lua_pushinteger(L, buff->cursor);
    return 1;
}

//code from https://github.com/LuaDist/lpack/blob/master/lpack.c
#define	OP_STRING	'A'
#define	OP_FLOAT	'f'
#define	OP_DOUBLE	'd'
#define	OP_NUMBER	'n'
#define	OP_CHAR		'c'
#define	OP_BYTE		'b'
#define	OP_SHORT	'h'
#define	OP_USHORT	'H'
#define	OP_INT		'i'
#define	OP_UINT		'I'
#define	OP_LONG		'l'
#define	OP_ULONG	'L'
#define	OP_LITTLEENDIAN	'<'
#define	OP_BIGENDIAN	'>'
#define	OP_NATIVE	'='

#define isdigit(c) ((c) >= '0' && (c) <= '9')
static void badcode(lua_State *L, int c)
{
    char s[]="bad code `?'";
    s[sizeof(s)-3]=c;
    luaL_argerror(L,1,s);
}
static int doendian(int c)
{
    int x=1;
    int e=*(char*)&x;
    if (c==OP_LITTLEENDIAN) return !e;
    if (c==OP_BIGENDIAN) return e;
    if (c==OP_NATIVE) return 0;
    return 0;
}
static void doswap(int swap, void *p, size_t n)
{
    if (swap)
    {
        char *a = p;
        int i, j;
        for (i = 0, j = n - 1, n = n / 2; n--; i++, j--)
        {
            char t = a[i];
            a[i] = a[j];
            a[j] = t;
        }
    }
}

/**
将一系列数据按照格式字符转化，并写入
@api buff:pack(format,val1, val2,...)
@string 后面数据的格式（符号含义见下面的例子）
@val  传入的数据，可以为多个数据
@return int 成功写入的数据长度
@usage
buff:pack(">IIHA", 0x1234, 0x4567, 0x12,"abcdefg") -- 按格式写入几个数据
-- A string
-- f float
-- d double
-- n Lua number
-- c char
-- b byte / unsignen char
-- h short
-- H unsigned short
-- i int
-- I unsigned int
-- l long
-- L unsigned long
-- < 小端
-- > 大端
-- = 默认大小端
 */
#define PACKNUMBER(OP,T)			\
    case OP:					\
    {						\
        T a=(T)luaL_checknumber(L,i++);		\
        doswap(swap,&a,sizeof(a));			\
        write_len += add_bytes(buff, (void*)&a, sizeof(a));\
        break;					\
    }
static int l_zbuff_pack(lua_State *L)
{
    luat_zbuff *buff = tozbuff(L);
    int i = 3;
    char *f = (char*)luaL_checkstring(L, 2);
    int swap = 0;
    int write_len = 0; //已写入长度
    while (*f)
    {
        if (buff->cursor == buff->len) //到头了
            break;
        int c = *f++;
        int N = 1;
        if (isdigit(*f))
        {
            N = 0;
            while (isdigit(*f))
                N = 10 * N + (*f++) - '0';
        }
        while (N--)
        {
            if (buff->cursor == buff->len) //到头了
                break;
            switch (c)
            {
            case OP_LITTLEENDIAN:
            case OP_BIGENDIAN:
            case OP_NATIVE:
            {
                swap = doendian(c);
                N = 0;
                break;
            }
            case OP_STRING:
            {
                size_t l;
                const char *a = luaL_checklstring(L, i++, &l);
                write_len += add_bytes(buff, a, l);
                break;
            }
            PACKNUMBER(OP_NUMBER, lua_Number)
            PACKNUMBER(OP_DOUBLE, double)
            PACKNUMBER(OP_FLOAT, float)
            PACKNUMBER(OP_CHAR, char)
            PACKNUMBER(OP_BYTE, unsigned char)
            PACKNUMBER(OP_SHORT, short)
            PACKNUMBER(OP_USHORT, unsigned short)
            PACKNUMBER(OP_INT, int)
            PACKNUMBER(OP_UINT, unsigned int)
            PACKNUMBER(OP_LONG, long)
            PACKNUMBER(OP_ULONG, unsigned long)
            case ' ':
            case ',':
                break;
            default:
                badcode(L, c);
                break;
            }
        }
    }
    lua_pushinteger(L, write_len);
    return 1;
}

#define UNPACKINT(OP,T)		\
    case OP:				\
    {					\
        T a;				\
        int m=sizeof(a);			\
        if (i+m>len) goto done;		\
        memcpy(&a,s+i,m);			\
        i+=m;				\
        doswap(swap,&a,m);			\
        lua_pushinteger(L,(lua_Integer)a);	\
        ++n;				\
        break;				\
    }
#define UNPACKNUMBER(OP,T)		\
    case OP:				\
    {					\
        T a;				\
        int m=sizeof(a);			\
        if (i+m>len) goto done;		\
        memcpy(&a,s+i,m);			\
        i+=m;				\
        doswap(swap,&a,m);			\
        lua_pushnumber(L,(lua_Number)a);	\
        ++n;				\
        break;				\
    }
/**
将一系列数据按照格式字符读取出来
@api buff:unpack(format)
@string 数据的格式（符号含义见上面pack接口的例子）
@return int 成功读取的数据字节长度
@return any 按格式读出来的数据
@usage
local cnt,a,b,c,s = buff:unpack(">IIHA10") -- 按格式读取几个数据
--如果全部成功读取，cnt就是4+4+2+10=20
 */
static int l_zbuff_unpack(lua_State *L)
{
    luat_zbuff *buff = tozbuff(L);
    char *f = (char *)luaL_checkstring(L, 2);
    size_t len = buff->len - buff->cursor;
    const char *s = buff->addr+buff->cursor;
    int i = 0;
    int n = 0;
    int swap = 0;
    while (*f)
    {
        int c = *f++;
        int N = 1;
        if (isdigit(*f))
        {
            N = 0;
            while (isdigit(*f))
                N = 10 * N + (*f++) - '0';
            if (N == 0 && c == OP_STRING)
            {
                lua_pushliteral(L, "");
                ++n;
            }
        }
        while (N--)
            switch (c)
            {
            case OP_LITTLEENDIAN:
            case OP_BIGENDIAN:
            case OP_NATIVE:
            {
                swap = doendian(c);
                N = 0;
                break;
            }
            case OP_STRING:
            {
                ++N;
                if (i + N > len)
                    goto done;
                lua_pushlstring(L, s + i, N);
                i += N;
                ++n;
                N = 0;
                break;
            }
            UNPACKNUMBER(OP_NUMBER, lua_Number)
            UNPACKNUMBER(OP_DOUBLE, double)
            UNPACKNUMBER(OP_FLOAT, float)
            UNPACKINT(OP_CHAR, char)
            UNPACKINT(OP_BYTE, unsigned char)
            UNPACKINT(OP_SHORT, short)
            UNPACKINT(OP_USHORT, unsigned short)
            UNPACKINT(OP_INT, int)
            UNPACKINT(OP_UINT, unsigned int)
            UNPACKINT(OP_LONG, long)
            UNPACKINT(OP_ULONG, unsigned long)
            case ' ':
            case ',':
                break;
            default:
                badcode(L, c);
                break;
            }
    }
done:
    buff->cursor += i;
    lua_pushinteger(L, i);
    lua_replace(L,-n-2);
    return n + 1;
}

/**
读取一个指定类型的数据
@api buff:read类型()
@注释 读取类型可为：I8、U8、I16、U16、I32、U32、I64、U64、F32、F64
@return number 读取的数据，如果越界则为nil
@usage
local data = buff:readI8()
local data = buff:readU32()
*/
#define zread(n,t,f) static int l_zbuff_read_##n(lua_State *L)\
{\
    luat_zbuff *buff = tozbuff(L);\
    if(buff->len - buff->cursor < sizeof(t))\
        return 0;\
    lua_push##f(L,*((t*)(buff->addr+buff->cursor)));\
    buff->cursor+=sizeof(t);\
    return 1;\
}
zread(i8, int8_t,  integer)
zread(u8, uint8_t, integer)
zread(i16,int16_t, integer)
zread(u16,uint16_t,integer)
zread(i32,int32_t, integer)
zread(u32,uint32_t,integer)
zread(i64,int64_t, integer)
zread(u64,uint64_t,integer)
zread(f32,float, number)
zread(f64,double,number)



/**
写入一个指定类型的数据
@api buff:write类型()
@number 待写入的数据
@注释 写入类型可为：I8、U8、I16、U16、I32、U32、I64、U64、F32、F64
@return number 成功写入的长度
@usage
local len = buff:writeI8(10)
local len = buff:writeU32(1024)
*/
#define zwrite(n,t,f) static int l_zbuff_write_##n(lua_State *L)\
{\
    luat_zbuff *buff = tozbuff(L);\
    if(buff->len - buff->cursor < sizeof(t))\
    {\
        lua_pushinteger(L,0);\
        return 1;\
    }\
    *((t*)(buff->addr+buff->cursor)) = (t)luaL_check##f(L,2);\
    buff->cursor+=sizeof(t);\
    lua_pushinteger(L,sizeof(t));\
    return 1;\
}
zwrite(i8, int8_t,  integer)
zwrite(u8, uint8_t, integer)
zwrite(i16,int16_t, integer)
zwrite(u16,uint16_t,integer)
zwrite(i32,int32_t, integer)
zwrite(u32,uint32_t,integer)
zwrite(i64,int64_t, integer)
zwrite(u64,uint64_t,integer)
zwrite(f32,float, number)
zwrite(f64,double,number)


/**
以下标形式进行数据读写
@api buff[n]
@int 第几个数据，以0开始的下标（C标准）
@return number 该位置的数据
@usage
buff[0] = 0xc8
local data = buff[0]
 */
static int l_zbuff_index(lua_State *L)
{
    luat_zbuff** pp = luaL_checkudata(L, 1, LUAT_ZBUFF_TYPE);
    int i;

    luaL_getmetatable(L, LUAT_ZBUFF_TYPE);
    lua_pushvalue(L, 2);
    lua_rawget(L, -2);

    if ( lua_isnil(L, -1) ) {
        /* found no method, so get value from userdata. */
        luat_zbuff *buff = tozbuff(L);
        int o = luaL_checkinteger(L,2);
        if(o > buff->len) return 0;
        lua_pushinteger(L,buff->addr[o]);
        return 1;
    };
    return 1;
}

static int l_zbuff_toStr(lua_State *L)
{
    luat_zbuff *buff = tozbuff(L);
    int start = luaL_optinteger(L, 2, 0);
    int len = luaL_optinteger(L, 3, buff->len);
    lua_pushlstring(L, buff->addr + start, len);
    return 1;
}

static int l_zbuff_newindex(lua_State *L)
{
    if(lua_isinteger(L,2)){
        luat_zbuff *buff = tozbuff(L);
        if(lua_isinteger(L,2)){
            int o = luaL_checkinteger(L,2);
            int n = luaL_checkinteger(L,3) % 256;
            if(o > buff->len) return 0;
            buff->addr[o] = n;
        }
    }
    return 0;
}

// __gc
static int l_zbuff_gc(lua_State *L)
{
    luat_zbuff *buff = tozbuff(L);
    luat_heap_free(buff->addr);
    return 0;
}

static const luaL_Reg lib_zbuff[] = {
    {"write", l_zbuff_write},
    {"read", l_zbuff_read},
    {"seek", l_zbuff_seek},
    {"pack", l_zbuff_pack},
    {"unpack", l_zbuff_unpack},
    {"get", l_zbuff_index},
    {"readI8", l_zbuff_read_i8},
    {"readI16", l_zbuff_read_i16},
    {"readI32", l_zbuff_read_i32},
    {"readI64", l_zbuff_read_i64},
    {"readU8", l_zbuff_read_u8},
    {"readU16", l_zbuff_read_u16},
    {"readU32", l_zbuff_read_u32},
    {"readU64", l_zbuff_read_u64},
    {"readF32", l_zbuff_read_f32},
    {"readF64", l_zbuff_read_f64},
    {"writeI8", l_zbuff_write_i8},
    {"writeI16", l_zbuff_write_i16},
    {"writeI32", l_zbuff_write_i32},
    {"writeI64", l_zbuff_write_i64},
    {"writeU8", l_zbuff_write_u8},
    {"writeU16", l_zbuff_write_u16},
    {"writeU32", l_zbuff_write_u32},
    {"writeU64", l_zbuff_write_u64},
    {"writeF32", l_zbuff_write_f32},
    {"writeF64", l_zbuff_write_f64},
    {"toStr",    l_zbuff_toStr},
    {NULL, NULL}
};

static const luaL_Reg lib_zbuff_metamethods[] = {
    {"__index", l_zbuff_index},
    {"__newindex", l_zbuff_newindex},
    {"__gc", l_zbuff_gc},
    {NULL, NULL}
};


static void createmeta(lua_State *L)
{
    luaL_newmetatable(L, LUAT_ZBUFF_TYPE); /* create metatable for file handles */
    // lua_pushvalue(L, -1);                  /* push metatable */
    // lua_setfield(L, -2, "__index");        /* metatable.__index = metatable */
    // luaL_setfuncs(L, lib_zbuff, 0);        /* add file methods to new metatable */
    luaL_setfuncs(L, lib_zbuff_metamethods, 0);
    luaL_setfuncs(L, lib_zbuff, 0);
    lua_pop(L, 1);                         /* pop new metatable */
    //luaL_newlib(L, lib_zbuff);
}

#include "rotable.h"
static const rotable_Reg reg_zbuff[] =
    {
        {"create", l_zbuff_create, 0},
        {"SEEK_SET", NULL, ZBUFF_SEEK_SET},
        {"SEEK_CUR", NULL, ZBUFF_SEEK_CUR},
        {"SEEK_END", NULL, ZBUFF_SEEK_END},
        {NULL, NULL, 0}};

LUAMOD_API int luaopen_zbuff(lua_State *L)
{
    rotable_newlib(L, reg_zbuff);
    createmeta(L);
    return 1;
}
