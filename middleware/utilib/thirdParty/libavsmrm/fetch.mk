ifdef GIT_REPO

ifndef GIT_COMMIT
$(error GIT_COMMIT undefined!)
endif

GIT_DIR ?= src

define fetch_rules
fetch:: | $(GIT_DIR)
	cd $(GIT_DIR) && git fetch && \
    (git checkout --detach $(GIT_COMMIT) || git checkout --detach origin/$(GIT_COMMIT))
$(GIT_DIR):
	git clone $(GIT_REPO) $(GIT_DIR)
endef

$(eval $(fetch_rules))

# end of GIT_REPO
else ifdef SVN_REPO

ifndef SVN_REV
$(error SVN_REV undefined!)
endif

SVN_DIR ?= src

define fetch_rules
fetch::
	svn co $(SVN_REPO)@$(SVN_REV) $(SVN_DIR)
endef

$(eval $(fetch_rules))

# end of SVN_REPO
endif
