######################################################################
#
# kconfig-frontends
#
######################################################################

KCONFIGS_VERSION:=$(strip $(subst ",, $(BR2_KCONFIG_FRONTENDS_VERSION)))
KCONFIGS_SOURCE:=kconfig-frontends-$(KCONFIGS_VERSION).tar.bz2
KCONFIGS_BUILD:=$(TOOL_BUILD_DIR)/kconfig-frontends-$(KCONFIGS_VERSION)
KCONFIGS_SITE:=http://ymorin.is-a-geek.org/download/kconfig-frontends
KCONFIGS_CAT:=$(BZCAT)

ifeq ($(strip $(BR2_NCONF_FRONTEND)),y)
KCONFIGS_ENABLE_NCONF:=--enable-nconf
else
KCONFIGS_ENABLE_NCONF:=--disable-nconf
endif

ifeq ($(strip $(BR2_GCONF_FRONTEND)),y)
KCONFIGS_ENABLE_GCONF:=--enable-gconf
else
KCONFIGS_ENABLE_GCONF:=--disable-gconf
endif

ifeq ($(strip $(BR2_QCONF_FRONTEND)),y)
KCONFIGS_ENABLE_QCONF:=--enable-qconf
else
KCONFIGS_ENABLE_QCONF:=--disable-qconf
endif

$(DL_DIR)/$(KCONFIGS_SOURCE):
	mkdir -p $(DL_DIR)
	$(WGET) -P $(DL_DIR) $(KCONFIGS_SITE)/$(KCONFIGS_SOURCE)

$(KCONFIGS_BUILD)/.unpacked : $(DL_DIR)/$(KCONFIGS_SOURCE)
	mkdir -p $(TOOL_BUILD_DIR)
	$(BZCAT) $(DL_DIR)/$(KCONFIGS_SOURCE) | tar -C $(TOOL_BUILD_DIR) $(TAR_OPTIONS) -
	@#toolchain/patch-kernel.sh $(KCONFIGS_BUILD) toolchain/kconfig-frontends \*.patch
	touch $@

$(KCONFIGS_BUILD)/.configured : $(KCONFIGS_BUILD)/.unpacked
	(cd $(KCONFIGS_BUILD); \
		CC="$(HOSTCC)" \
		./configure \
		--enable-mconf \
		--disable-shared \
		--enable-static \
		--disable-utils \
		$(KCONFIGS_ENABLE_NCONF) \
		$(KCONFIGS_ENABLE_GCONF) \
		$(KCONFIGS_ENABLE_QCONF) \
		--prefix=$(STAGING_DIR) \
		--build=$(GNU_HOST_NAME) \
		--host=$(GNU_HOST_NAME) \
		--disable-werror);
	touch $@

$(KCONFIGS_BUILD)/.compiled : $(KCONFIGS_BUILD)/.configured
	$(MAKE) -C $(KCONFIGS_BUILD)
	touch $@

$(KCONFIGS_BUILD)/.installed: $(KCONFIGS_BUILD)/.compiled
	$(MAKE) -C $(KCONFIGS_BUILD) install
	touch $@

kconfig-frontends: $(KCONFIGS_BUILD)/.installed

kconfig-frontends-source:

kconfig-frontends-clean:
	rm -f $(STAGING_DIR)/bin/kconfig-conf
	rm -f $(STAGING_DIR)/bin/kconfig-mconf
	rm -f $(STAGING_DIR)/bin/kconfig-nconf
	rm -f $(STAGING_DIR)/bin/kconfig-gconf
	rm -f $(STAGING_DIR)/bin/kconfig-qconf
	(if [ -d $(KCONFIGS_BUILD) ]; then $(MAKE) -C $(KCONFIGS_BUILD) clean; fi)
	rm -f $(KCONFIGS_BUILD)/.installed
	rm -f $(KCONFIGS_BUILD)/.compiled
	rm -f $(KCONFIGS_BUILD)/.configured
	rm -f $(KCONFIGS_BUILD)/.unpacked

kconfig-frontends-dirclean:
	rm -rf $(KCONFIGS_BUILD)

ifeq ($(strip $(BR2_PACKAGE_KCONFIG_FRONTENDS)),y)
TARGETS+=kconfig-frontends
endif
