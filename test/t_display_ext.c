#include "test.h"

#include "display_driver.h"
#include "display_ext.h"
#include "mem.h"

static int t_result;
static int t_calls;

static int t_ext_init(display_ext_t *ext, strv_t name)
{
	(void)name;
	t_calls++;
	ext->opcode	 = 128;
	ext->first_event = 64;
	ext->first_error = 32;
	return t_result;
}

static int t_ext_send(display_ext_t *ext, u8 opcode, const void *data, size_t size)
{
	(void)ext;
	(void)opcode;
	(void)data;
	(void)size;
	t_calls++;
	return t_result;
}

static int t_ext_call(display_ext_t *ext, u8 opcode, const void *data, size_t size, display_ext_reply_t *reply)
{
	(void)ext;
	(void)opcode;
	(void)data;
	(void)size;
	t_calls++;
	reply->header[0] = 1;
	reply->size	 = 4;
	reply->data	 = mem_alloc(reply->size);
	return t_result;
}

static int t_alloc_id(display_t *display, u32 *id)
{
	(void)display;
	t_calls++;
	*id = 42;
	return t_result;
}

static int t_visual_depth(display_t *display, u32 visual, u8 *depth)
{
	(void)display;
	(void)visual;
	t_calls++;
	*depth = 24;
	return t_result;
}

static display_driver_t t_driver = {
	.ext_init     = t_ext_init,
	.ext_send     = t_ext_send,
	.ext_call     = t_ext_call,
	.alloc_id     = t_alloc_id,
	.visual_depth = t_visual_depth,
};

static void t_reset(void)
{
	t_result = 0;
	t_calls	 = 0;
}

TEST(display_ext_init_sets_extension)
{
	START;

	t_reset();
	display_t display = {.drv = &t_driver};
	display_ext_t ext = {0};

	EXPECT_EQ(display_ext_init(&ext, &display, STRV("test")), &ext);
	EXPECT_EQ(ext.display, &display);
	EXPECT_EQ(ext.opcode, 128);
	EXPECT_EQ(ext.first_event, 64);
	EXPECT_EQ(ext.first_error, 32);
	EXPECT_EQ(t_calls, 1);

	END;
}

TEST(display_ext_init_clears_failed_extension)
{
	START;

	t_reset();
	t_result	  = 1;
	display_t display = {.drv = &t_driver};
	display_ext_t ext = {.opcode = 1, .first_event = 2, .first_error = 3};

	EXPECT_EQ(display_ext_init(&ext, &display, STRV("test")), NULL);
	EXPECT_EQ(ext.display, NULL);
	EXPECT_EQ(ext.opcode, 0);
	EXPECT_EQ(ext.first_event, 0);
	EXPECT_EQ(ext.first_error, 0);

	END;
}

TEST(display_ext_init_rejects_invalid_arguments)
{
	START;

	display_driver_t driver = {0};
	display_t no_driver	= {0};
	display_t no_init	= {.drv = &driver};
	display_ext_t ext	= {0};

	EXPECT_EQ(display_ext_init(NULL, &no_init, STRV("test")), NULL);
	EXPECT_EQ(display_ext_init(&ext, NULL, STRV("test")), NULL);
	EXPECT_EQ(display_ext_init(&ext, &no_driver, STRV("test")), NULL);
	EXPECT_EQ(display_ext_init(&ext, &no_init, STRV("test")), NULL);
	EXPECT_EQ(display_ext_init(&ext, &(display_t){.drv = &t_driver}, (strv_t){0}), NULL);
	EXPECT_EQ(display_ext_init(&ext, &(display_t){.drv = &t_driver}, STRVN("", 0)), NULL);

	END;
}

TEST(display_ext_send_dispatches_request)
{
	START;

	t_reset();
	display_t display = {.drv = &t_driver};
	display_ext_t ext = {.display = &display};
	u8 data[]	  = {1, 2, 3, 4};

	EXPECT_EQ(display_ext_send(&ext, 7, data, sizeof(data)), 0);
	EXPECT_EQ(t_calls, 1);

	END;
}

TEST(display_ext_send_returns_driver_result)
{
	START;

	t_reset();
	t_result	  = 1;
	display_t display = {.drv = &t_driver};
	display_ext_t ext = {.display = &display};

	EXPECT_EQ(display_ext_send(&ext, 7, NULL, 0), 1);

	END;
}

TEST(display_ext_send_rejects_invalid_request)
{
	START;

	display_driver_t driver = {0};
	display_t display	= {.drv = &driver};
	display_ext_t ext	= {.display = &display};

	EXPECT_EQ(display_ext_send(NULL, 0, NULL, 0), 1);
	EXPECT_EQ(display_ext_send(&(display_ext_t){0}, 0, NULL, 0), 1);
	EXPECT_EQ(display_ext_send(&ext, 0, NULL, 0), 1);
	ext.display->drv = &t_driver;
	EXPECT_EQ(display_ext_send(&ext, 0, NULL, 1), 1);

	END;
}

TEST(display_ext_call_sets_reply)
{
	START;

	t_reset();
	display_t display = {.drv = &t_driver};
	display_ext_t ext = {.display = &display};
	display_ext_reply_t reply;

	EXPECT_EQ(display_ext_call(&ext, 2, NULL, 0, &reply), 0);
	EXPECT_EQ(reply.header[0], 1);
	EXPECT_EQ(reply.size, 4);
	EXPECT_NE(reply.data, NULL);
	EXPECT_EQ(t_calls, 1);
	display_ext_reply_free(&reply);

	END;
}

TEST(display_ext_call_frees_failed_reply)
{
	START;

	t_reset();
	t_result	  = 1;
	display_t display = {.drv = &t_driver};
	display_ext_t ext = {.display = &display};
	display_ext_reply_t reply;

	EXPECT_EQ(display_ext_call(&ext, 2, NULL, 0, &reply), 1);
	EXPECT_EQ(reply.header[0], 0);
	EXPECT_EQ(reply.size, 0);
	EXPECT_EQ(reply.data, NULL);

	END;
}

TEST(display_ext_call_rejects_invalid_call)
{
	START;

	display_driver_t driver = {0};
	display_t display	= {.drv = &driver};
	display_ext_t ext	= {.display = &display};
	display_ext_reply_t reply;

	EXPECT_EQ(display_ext_call(NULL, 0, NULL, 0, &reply), 1);
	EXPECT_EQ(display_ext_call(&(display_ext_t){0}, 0, NULL, 0, &reply), 1);
	EXPECT_EQ(display_ext_call(&ext, 0, NULL, 0, &reply), 1);
	ext.display->drv = &t_driver;
	EXPECT_EQ(display_ext_call(&ext, 0, NULL, 0, NULL), 1);
	EXPECT_EQ(display_ext_call(&ext, 0, NULL, 1, &reply), 1);

	END;
}

TEST(display_ext_reply_free_clears_reply)
{
	START;

	display_ext_reply_t reply = {.data = mem_alloc(4), .size = 4};
	reply.header[0]		  = 1;

	display_ext_reply_free(&reply);
	EXPECT_EQ(reply.header[0], 0);
	EXPECT_EQ(reply.data, NULL);
	EXPECT_EQ(reply.size, 0);

	END;
}

TEST(display_ext_reply_free_accepts_null)
{
	START;

	display_ext_reply_free(NULL);

	END;
}

TEST(display_alloc_id_dispatches_request)
{
	START;

	t_reset();
	display_t display = {.drv = &t_driver};
	u32 id;

	EXPECT_EQ(display_alloc_id(&display, &id), 0);
	EXPECT_EQ(id, 42);
	EXPECT_EQ(t_calls, 1);

	END;
}

TEST(display_alloc_id_rejects_invalid_arguments)
{
	START;

	display_driver_t driver = {0};

	EXPECT_EQ(display_alloc_id(NULL, &(u32){0}), 1);
	EXPECT_EQ(display_alloc_id(&(display_t){0}, &(u32){0}), 1);
	EXPECT_EQ(display_alloc_id(&(display_t){.drv = &driver}, &(u32){0}), 1);
	EXPECT_EQ(display_alloc_id(&(display_t){.drv = &t_driver}, NULL), 1);

	END;
}

TEST(display_visual_depth_dispatches_request)
{
	START;

	t_reset();
	display_t display = {.drv = &t_driver};
	u8 depth;

	EXPECT_EQ(display_visual_depth(&display, 7, &depth), 0);
	EXPECT_EQ(depth, 24);
	EXPECT_EQ(t_calls, 1);

	END;
}

TEST(display_visual_depth_rejects_invalid_arguments)
{
	START;

	display_driver_t driver = {0};

	EXPECT_EQ(display_visual_depth(NULL, 1, &(u8){0}), 1);
	EXPECT_EQ(display_visual_depth(&(display_t){0}, 1, &(u8){0}), 1);
	EXPECT_EQ(display_visual_depth(&(display_t){.drv = &driver}, 1, &(u8){0}), 1);
	EXPECT_EQ(display_visual_depth(&(display_t){.drv = &t_driver}, 0, &(u8){0}), 1);
	EXPECT_EQ(display_visual_depth(&(display_t){.drv = &t_driver}, 1, NULL), 1);

	END;
}

STEST(display_ext)
{
	SSTART;
	RUN(display_ext_init_sets_extension);
	RUN(display_ext_init_clears_failed_extension);
	RUN(display_ext_init_rejects_invalid_arguments);
	RUN(display_ext_send_dispatches_request);
	RUN(display_ext_send_returns_driver_result);
	RUN(display_ext_send_rejects_invalid_request);
	RUN(display_ext_call_sets_reply);
	RUN(display_ext_call_frees_failed_reply);
	RUN(display_ext_call_rejects_invalid_call);
	RUN(display_ext_reply_free_clears_reply);
	RUN(display_ext_reply_free_accepts_null);
	RUN(display_alloc_id_dispatches_request);
	RUN(display_alloc_id_rejects_invalid_arguments);
	RUN(display_visual_depth_dispatches_request);
	RUN(display_visual_depth_rejects_invalid_arguments);
	SEND;
}
