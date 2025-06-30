

install-spx-ui-assets:
	@echo "Installing SPX web UI to: $(INSTALL_ROOT)$(PHP_SPX_ASSETS_DIR)/web-ui"
	@mkdir -p $(INSTALL_ROOT)$(PHP_SPX_ASSETS_DIR)/web-ui
	@cp -r assets/web-ui/* $(INSTALL_ROOT)$(PHP_SPX_ASSETS_DIR)/web-ui

install: $(all_targets) $(install_targets) install-spx-ui-assets
