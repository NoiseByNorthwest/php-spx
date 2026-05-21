

install-spx-ui-assets:
	@if [ "$(PHP_SPX_SYMLINK_ASSETS_DIR)" = 'yes' ]; then \
		echo "Installing SPX web UI: symlink $(top_srcdir)/assets/web-ui to $(INSTALL_ROOT)$(PHP_SPX_ASSETS_DIR)/web-ui"; \
		if [ -d "$(INSTALL_ROOT)$(PHP_SPX_ASSETS_DIR)/web-ui" ] && ! [ -L "$(INSTALL_ROOT)$(PHP_SPX_ASSETS_DIR)/web-ui" ]; then \
			rm -rf "$(INSTALL_ROOT)$(PHP_SPX_ASSETS_DIR)/web-ui"; \
		fi; \
		ln -sfn "$(top_srcdir)/assets/web-ui" "$(INSTALL_ROOT)$(PHP_SPX_ASSETS_DIR)/web-ui"; \
	else \
		echo "Installing SPX web UI to: $(INSTALL_ROOT)$(PHP_SPX_ASSETS_DIR)/web-ui"; \
		mkdir -p "$(INSTALL_ROOT)$(PHP_SPX_ASSETS_DIR)/web-ui"; \
		cp -r assets/web-ui/* "$(INSTALL_ROOT)$(PHP_SPX_ASSETS_DIR)/web-ui"; \
	fi

install: $(all_targets) $(install_targets) install-spx-ui-assets
