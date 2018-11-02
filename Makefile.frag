
spx_ui_assets_dir = $(INSTALL_ROOT)$(prefix)/share/misc/php-spx/assets/web-ui

CFLAGS += -DSPX_HTTP_UI_ASSETS_DIR=\"$(spx_ui_assets_dir)\"

install-spx-ui-assets:
	@echo "Installing SPX web UI to: $(spx_ui_assets_dir)"
	@mkdir -p $(spx_ui_assets_dir)
	@cp -r assets/web-ui/* $(spx_ui_assets_dir)

install: $(all_targets) $(install_targets) install-spx-ui-assets
