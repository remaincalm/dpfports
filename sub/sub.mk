######################################
#
# sub
#
######################################

# where to find the source code - locally in this case
SUB_SITE_METHOD = local
SUB_SITE = $($(PKG)_PKGDIR)/

# even though this is a local build, we still need a version number
# bump this number if you need to force a rebuild
SUB_VERSION = 1

# dependencies (list of other buildroot packages, separated by space)
# on this package we need to depend on the host version of ourselves to be able to run the ttl generator
SUB_DEPENDENCIES = host-sub

# LV2 bundles that this package generates (space separated list)
SUB_BUNDLES = sub.lv2

# call make with the current arguments and path. "$(@D)" is the build directory.
SUB_HOST_MAKE   = $(HOST_MAKE_ENV)   $(HOST_CONFIGURE_OPTS)   $(MAKE) -C $(@D)/source
SUB_TARGET_MAKE = $(TARGET_MAKE_ENV) $(TARGET_CONFIGURE_OPTS) $(MAKE) -C $(@D)/source

# temp dir where we place the generated ttls
SUB_TMP_DIR = $(HOST_DIR)/tmp-sub


# build plugins in host to generate ttls
define HOST_SUB_BUILD_CMDS
	# build everything
	$(SUB_HOST_MAKE)

	# delete binaries
	rm $(@D)/source/build/*.lv2/*.so

	# create temp dir
	rm -rf $(SUB_TMP_DIR)
	mkdir -p $(SUB_TMP_DIR)

	# copy the generated bundles without binaries to temp dir
	cp -r $(@D)/source/build/*.lv2 $(SUB_TMP_DIR)
endef

# build plugins in target skipping ttl generation
define SUB_BUILD_CMDS
	# create dummy generator
	mkdir -p $(@D)/source/build
	touch $(@D)/source/build/lv2_ttl_generator
	chmod +x $(@D)/source/build/lv2_ttl_generator

	# copy previously generated bundles
	cp -r $(SUB_TMP_DIR)/*.lv2 $(@D)/source/build/

	# now build in target
	$(SUB_TARGET_MAKE)

	# cleanup
	rm $(@D)/source/build/lv2_ttl_generator
	rm -r $(SUB_TMP_DIR)
endef

# install command
define SUB_INSTALL_TARGET_CMDS
	$(SUB_TARGET_MAKE) install DESTDIR=$(TARGET_DIR)
endef


# import everything else from the buildroot generic package
$(eval $(generic-package))
# import host version too
$(eval $(host-generic-package))
