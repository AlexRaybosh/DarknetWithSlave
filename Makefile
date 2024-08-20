where-am-i = $(CURDIR)/$(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST))
THIS_MAKEFILE := $(call where-am-i)
ROOT:=$(dir $(THIS_MAKEFILE))

ROOT:=$(shell echo $(ROOT) | sed -e 's/\/\//\//g' -e 's/\/$$//g')
ARCH:=$(shell uname -m | tr -d '\n')

BCONC:=1
NNPACK_BACKEND:=auto
DARKNET_EXTRA_OPT:=
ifeq "$(ARCH)" "aarch64"
NNPACK_BACKEND:=neon
#NNPACK_BACKEND:=psimd
BCONC:=4
DARKNET_EXTRA_OPT:="-mtune=native -ftree-vectorize -Ofast -fomit-frame-pointer  -march=armv8-a+simd -mcpu=cortex-a72+simd "
endif

ifeq "$(ARCH)" "x86_64"
NNPACK_BACKEND:=auto
BCONC:=4
DARKNET_EXTRA_OPT:="-march=native -mtune=native -ftree-vectorize -Ofast -fomit-frame-pointer"
#DARKNET_EXTRA_OPT:="-O0 -g"
endif

 

NNPACK_SRC:=$(ROOT)/NNPACK-darknet
BUILDDIR:=$(ROOT)/build/$(ARCH)
NNPACK_BUILDDIR:=$(BUILDDIR)/nnpack
PREBUILD:=$(ROOT)/prebuild/$(ARCH)
INCLUDE:=$(ROOT)/include
POSTERITY:=$(ROOT)/posterity/$(ARCH)

DARKNET_SRC:=$(ROOT)/darknet-nnpack
DARKNET_BUILDDIR:=$(BUILDDIR)/darknet


$(info ARCH $(ARCH))
$(info ROOT $(ROOT))
$(info BUILDDIR $(BUILDDIR))
$(info NNPACK_BUILDDIR $(NNPACK_BUILDDIR))
$(info NNPACK_SRC $(NNPACK_SRC))
$(info DARKNET_BUILDDIR $(DARKNET_BUILDDIR))


all: $(DARKNET_BUILDDIR)/build.$(ARCH)

NNPACK:
	@mkdir -p $(NNPACK_BUILDDIR)
	@cp $(NNPACK_SRC)/deps/fp16/include/fp16/psimd.h $(NNPACK_BUILDDIR) #cmake shit refreshes git and drops it
	@cd $(NNPACK_BUILDDIR) && cmake -DCMAKE_BUILD_TYPE=Release -DNNPACK_BACKEND=$(NNPACK_BACKEND) $(NNPACK_SRC)
	@mv -f $(NNPACK_BUILDDIR)/psimd.h $(NNPACK_SRC)/deps/fp16/include/fp16/psimd.h # move it back
	@cd $(NNPACK_BUILDDIR) && make -j $(BCONC)
	@mkdir -p $(PREBUILD)
	@cp $(NNPACK_BUILDDIR)/libnnpack_reference_layers.a $(PREBUILD)
	@cp $(NNPACK_BUILDDIR)/libnnpack.a $(PREBUILD)
	@cp $(NNPACK_BUILDDIR)/deps/cpuinfo/libcpuinfo_internals.a $(PREBUILD)
	@cp $(NNPACK_BUILDDIR)/deps/cpuinfo/libcpuinfo.a $(PREBUILD)
	@cp $(NNPACK_BUILDDIR)/deps/pthreadpool/libpthreadpool.a $(PREBUILD)
	@cp $(NNPACK_SRC)/include/nnpack.h $(INCLUDE)
	@cp $(NNPACK_SRC)/deps/pthreadpool/include/pthreadpool.h $(INCLUDE)
	@mkdir -p $(POSTERITY)
	@cd $(NNPACK_BUILDDIR) && tar -czf $(POSTERITY)/nnpack-build-last-success.tgz . # hence cmake shit will go bad at some point


$(PREBUILD)/libnnpack.a:
	$(MAKE) NNPACK
	
$(DARKNET_BUILDDIR)/build.$(ARCH):
	$(MAKE) DARKNET


DARKNET_SRC_FILES:=$(wildcard $(DARKNET_SRC)/src/*.c $(DARKNET_SRC)/src/*.h $(DARKNET_SRC)/examples/*.c)
#$(info DARKNET_SRC_FILES $(DARKNET_SRC_FILES))
	
	
DARKNET: $(PREBUILD)/libnnpack.a
	cd $(DARKNET_SRC) && make -j $(BCONC) ROOT=$(ROOT) PREBUILD=$(PREBUILD) DARKNET_EXTRA_OPT=$(DARKNET_EXTRA_OPT)
	@cp $(DARKNET_SRC)/libdarknet.a $(PREBUILD)
	@cp $(DARKNET_SRC)/darknet $(PREBUILD)
	@cp $(DARKNET_SRC)/darkslave $(PREBUILD)
	@cp $(DARKNET_SRC)/include/darknet.h $(INCLUDE)
	@mkdir -p $(DARKNET_BUILDDIR)
	#@cd $(DARKNET_SRC) && tar -cf - . | ( cd $(DARKNET_BUILDDIR) && tar -xf - )
	#@cd $(DARKNET_BUILDDIR) && make -j $(BCONC) ROOT=$(ROOT) PREBUILD=$(PREBUILD) DARKNET_EXTRA_OPT=$(DARKNET_EXTRA_OPT)
	#@cp $(DARKNET_BUILDDIR)/libdarknet.a $(PREBUILD)
	#@cp $(DARKNET_BUILDDIR)/libdarknet.so $(PREBUILD)
	#@cp $(DARKNET_BUILDDIR)/darknet $(PREBUILD)
	#@cp $(DARKNET_BUILDDIR)/include/darknet.h $(INCLUDE)
	@touch $(DARKNET_BUILDDIR)/build.$(ARCH)

	

clean:
	@rm -rf $(BUILDDIR)
	$(MAKE) -C $(DARKNET_SRC) clean

.PHONY: NNPACK clean DARKNET