#include <dlfcn.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "leptonet_malloc.h"
#include "leptonet_module.h"
#include "spinlock.h"

#define MAX_MODULES 256

struct modules {
  size_t cnt;         // module cnt 
  const char *path;   // module path, lua format
  struct spinlock lock;
  struct leptonet_module mods[MAX_MODULES];
};

static struct modules *M = NULL;

static void* leptonet_strdup(const char *str) {
  int len = strlen(str);
  char * s = leptonet_malloc(len);
  memcpy(s, str, len);
  return s;
}

void leptonet_module_init(const char *path) {
  struct modules *m = leptonet_malloc(sizeof *m);
  memset(m, 0, sizeof *m);
  spinlock_init(&m->lock);
  // module manager need to manage this path by itself
  m->path = leptonet_strdup(path);
  M = m;
}

static struct leptonet_module* query_module(const char *name) {
  for (int i = 0; i < MAX_MODULES; i ++) {
    if (strcmp(M->mods[i].name, name) == 0) {
      return &M->mods[i];
    } 
  }
  return NULL;
}

static struct leptonet_module* try_open(struct modules *mod, const char *name) {
  // M->path can be:
  // ./?.lua;./?/init.lua;...
  const char *path = mod->path;
  size_t path_size = strlen(path);
  size_t name_size = strlen(name);
  size_t sz = path_size + name_size;
  char buf[sz];
  void *dl = NULL;
  do {
    memset(buf, 0, sz);
    while(*path == ';') path++;
    if (*path == '\0') {
      break;
    }
    char *l = strchr(path, ';');
    if (l == NULL) {
      l = (char*)path + strlen(path);
    }
    size_t len = l - path;
    int i = 0;
    while (i < len && path[i] != '?') {
      buf[i] = path[i];
      i++;
    }
    if (path[i] != '?') {
      fprintf(stderr, "[leptonet-module]: filepath error: %s\n", path);
      continue;
    }
    memcpy(buf + i, name, name_size);
    memcpy(buf + i + name_size, path + i, len - i - 1);
    dl = dlopen(buf, RTLD_NOW | RTLD_GLOBAL);
    if (dl == NULL) {
      fprintf(stderr, "[leptonet-module]: dlopen error: %s\n", dlerror());
    }
  } while(dl == NULL);
  if(dl == NULL) {
    fprintf(stderr, "[leptonet-module]: try to open: %s but error: %s\n", name, dlerror());
  }
  return dl;
}

static void* get_api(struct leptonet_module *mod, const char *api) {
  size_t name_size = strlen(mod->name);
  size_t api_size = strlen(api);
  // suppose M->path is ./?.lua;./?/init.lua
  // suppose mod->name is logger
  // so, in logger.so, we must provide logger_create, logger_init, logger_free, logger_signal
  char buf[name_size + api_size + 1];
  memcpy(buf, mod->name, name_size);
  // we must copy the last '\0'
  memcpy(buf + name_size, api, api_size + 1);
  return dlsym(mod->module, buf);
}

static int load_sym(struct leptonet_module *mod) {
  mod->create = get_api(mod, "_create");
  mod->init = get_api(mod, "_init");
  mod->free = get_api(mod, "_free");
  mod->signal = get_api(mod, "_signal");
  return mod->init != NULL;
}

struct leptonet_module* leptonet_module_query(const char *name) {
  spinlock_lock(&M->lock);
  struct leptonet_module* module = query_module(name);
  if (module) {
    spinlock_unlock(&M->lock);
    return module;
  } else if (M->cnt < MAX_MODULES) {
    module = try_open(M, name);  
  }
  if (module) {
    // we temporary assign these two value to call load_sym()
    M->mods[M->cnt].name = name;
    M->mods[M->cnt].module = module;
    if (load_sym(&M->mods[M->cnt])) {
      M->mods[M->cnt].name = leptonet_strdup(name);
      M->cnt++; 
    }
  }
  spinlock_unlock(&M->lock);
  return module;
}

void* leptonet_module_instance_create(struct leptonet_module *mod) {
  if (mod->create) {
    return mod->create();
  }
  return NULL;
}

int leptonet_module_instance_init(struct leptonet_module *mod, void *inst, struct leptonet_context *ctx, const char *param) {
  return mod->init(inst, ctx, param);
}

void leptonet_module_instance_free(struct leptonet_module *mod, void *inst) {
  if (mod->free) {
    mod->free(inst);
  }
}

void leptonet_module_instance_signal(struct leptonet_module *mod, void *inst, int sig) {
  if (mod->signal) {
    mod->signal(inst, sig);
  }
}

