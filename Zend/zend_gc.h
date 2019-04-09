/*
   +----------------------------------------------------------------------+
   | Zend Engine                                                          |
   +----------------------------------------------------------------------+
   | Copyright (c) 1998-2016 Zend Technologies Ltd. (http://www.zend.com) |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.00 of the Zend license,     |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.zend.com/license/2_00.txt.                                |
   | If you did not receive a copy of the Zend license and are unable to  |
   | obtain it through the world-wide-web, please send a note to          |
   | license@zend.com so we can mail you a copy immediately.              |
   +----------------------------------------------------------------------+
   | Authors: David Wang <planetbeing@gmail.com>                          |
   |          Dmitry Stogov <dmitry@zend.com>                             |
   +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef ZEND_GC_H
#define ZEND_GC_H

#ifndef GC_BENCH
# define GC_BENCH 0
#endif

#if GC_BENCH
# define GC_BENCH_INC(counter) GC_G(counter)++
# define GC_BENCH_DEC(counter) GC_G(counter)--
# define GC_BENCH_PEAK(peak, counter) do {		\
		if (GC_G(counter) > GC_G(peak)) {		\
			GC_G(peak) = GC_G(counter);			\
		}										\
	} while (0)
#else
# define GC_BENCH_INC(counter)
# define GC_BENCH_DEC(counter)
# define GC_BENCH_PEAK(peak, counter)
#endif

#define GC_COLOR  0xc000 /* 颜色标志位 */

#define GC_BLACK  0x0000 /* 黑色 */
#define GC_WHITE  0x8000 /* 白色 */
#define GC_GREY   0x4000 /* 灰色 */
#define GC_PURPLE 0xc000 /* 紫色 */

#define GC_ADDRESS(v) \
	((v) & ~GC_COLOR)
#define GC_INFO_GET_COLOR(v) \
	(((zend_uintptr_t)(v)) & GC_COLOR)
#define GC_INFO_SET_ADDRESS(v, a) \
	do {(v) = ((v) & GC_COLOR) | (a);} while (0)
#define GC_INFO_SET_COLOR(v, c) \
	do {(v) = ((v) & ~GC_COLOR) | (c);} while (0)
#define GC_INFO_SET_BLACK(v) \
	do {(v) = (v) & ~GC_COLOR;} while (0)
#define GC_INFO_SET_PURPLE(v) \
	do {(v) = (v) | GC_COLOR;} while (0)

/**
 * 双向列表
 * 记录引用技术的相关信息
 */
typedef struct _gc_root_buffer {
	zend_refcounted          *ref;
	struct _gc_root_buffer   *next;     /* double-linked list               */
	struct _gc_root_buffer   *prev;
	uint32_t                 refcount;
} gc_root_buffer;

/**
 * 垃圾收集器
 * 垃圾回收包含：垃圾收集器和垃圾回收算法
 * 总大小 120 字节
 */
typedef struct _zend_gc_globals {
	zend_bool         gc_enabled; /* 是否开启 gc */
	zend_bool         gc_active;  /* 垃圾回收算法是否运行 */
	zend_bool         gc_full;    /* 垃圾缓冲区是否满了，在 debug 模式下有用 */

	gc_root_buffer   *buf;				/* preallocated arrays of buffers   | 垃圾缓冲区。PHP7 默认大小位 10,000 个节点位置  */
	gc_root_buffer    roots;			/* list of possible roots of cycles | 指向缓冲区最新加入的可能是垃圾的元素 */
	gc_root_buffer   *unused;			/* list of unused buffers           | 未使用的缓冲区列表  */
	gc_root_buffer   *first_unused;		/* pointer to first unused buffer   | 指向第一个未使用的缓冲区   */
	gc_root_buffer   *last_unused;		/* pointer to last unused buffer    | 指向最后一个未使用的缓冲区  */

	gc_root_buffer    to_free;			/* list to free                     | 待释放的列表 */
	gc_root_buffer   *next_to_free;     /* 下一个待释放的列表 */

	uint32_t gc_runs;   /* 记录垃圾回收算法运行的次数，当缓冲区满了，才会运行 gc 算法 */
	uint32_t collected; /* 记录垃圾回收算法回收的垃圾数 */

#if GC_BENCH
	uint32_t root_buf_length;
	uint32_t root_buf_peak;
	uint32_t zval_possible_root;
	uint32_t zval_buffered;
	uint32_t zval_remove_from_buffer;
	uint32_t zval_marked_grey;
#endif

} zend_gc_globals;

#ifdef ZTS
BEGIN_EXTERN_C()
ZEND_API extern int gc_globals_id;
END_EXTERN_C()
#define GC_G(v) ZEND_TSRMG(gc_globals_id, zend_gc_globals *, v)
#else
#define GC_G(v) (gc_globals.v)
extern ZEND_API zend_gc_globals gc_globals;
#endif

BEGIN_EXTERN_C()
ZEND_API extern int (*gc_collect_cycles)(void);

ZEND_API void ZEND_FASTCALL gc_possible_root(zend_refcounted *ref);
ZEND_API void ZEND_FASTCALL gc_remove_from_buffer(zend_refcounted *ref);
ZEND_API void gc_globals_ctor(void);
ZEND_API void gc_globals_dtor(void);
ZEND_API void gc_init(void);
ZEND_API void gc_reset(void);

/* The default implementation of the gc_collect_cycles callback. */
ZEND_API int  zend_gc_collect_cycles(void);
END_EXTERN_C()

#define GC_ZVAL_CHECK_POSSIBLE_ROOT(z) \
	gc_check_possible_root((z))

#define GC_REMOVE_FROM_BUFFER(p) do { \
		zend_refcounted *_p = (zend_refcounted*)(p); \
		if (GC_ADDRESS(GC_INFO(_p))) { \
			gc_remove_from_buffer(_p); \
		} \
	} while (0)

static zend_always_inline void gc_check_possible_root(zval *z)
{
	ZVAL_DEREF(z);
	if (Z_COLLECTABLE_P(z) && UNEXPECTED(!Z_GC_INFO_P(z))) {
		gc_possible_root(Z_COUNTED_P(z));
	}
}

#endif /* ZEND_GC_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 */
