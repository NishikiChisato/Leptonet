#ifndef __LEPTONET_MODULE_H__
#define __LEPTONET_MODULE_H__

struct leptonet_context;

// return module instance
typedef void* (*leptonet_dl_create)(void);
// return value indicate init success or not
// use inst, returned by leptonet_dl_create, leptonet_context, parameter to init this module
typedef int (*leptonet_dl_init)(void *inst, struct leptonet_context *ctx, const char *param);
// release this module
typedef void (*leptonet_dl_free)(void *inst);
// a way to manually send a signal to this module
typedef void (*leptonet_dl_signal)(void *inst, int sig);

struct leptonet_module {
  const char *name;         // module name, used to identify module
  void *module;               // module instance, getted from dlopen
  leptonet_dl_create create;
  leptonet_dl_init init;
  leptonet_dl_free free;
  leptonet_dl_signal signal;
};

void leptonet_module_init(const char *path);

struct leptonet_module* leptonet_module_query(const char *name);

void* leptonet_module_instance_create(struct leptonet_module *mod);

int leptonet_module_instance_init(struct leptonet_module *mod, void *inst, struct leptonet_context *ctx, const char *param);

void leptonet_module_instance_free(struct leptonet_module *mod, void *inst);

void leptonet_module_instance_signal(struct leptonet_module *mod, void *inst, int sig);

#endif
