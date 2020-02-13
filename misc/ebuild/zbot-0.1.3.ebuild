# Copyright 1999-2020 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

EAPI=6

inherit cmake-utils git-r3 user

DESCRIPTION=""
HOMEPAGE="https://github.com/DanteG41/zbot"
EGIT_REPO_URI="https://github.com/DanteG41/zbot.git"
EGIT_COMMIT="v${PV}"
EGIT_SUBMODULES=( libs/simpleini libs/tgbot )
LICENSE="GPL-3.0"
SLOT="0"
KEYWORDS="~amd64 ~x86"

DEPEND=""

pkg_setup() {
	enewgroup	zbot 80
	enewuser	zbot 80 /bin/bash /var/empty zbot

}

src_install() {
	cmake-utils_src_install

	newconfd	"${FILESDIR}/zbotd-confd-r1" "zbotd"
	newinitd	"${FILESDIR}/zbotd-initd-r1" "zbotd"

	insinto		/etc/zbot
	newins		misc/sample_config.ini zbot.ini
}

pkg_postinst() {
	if [[ ! -d /var/spool/zbot ]]; then
			ewarn
			ewarn "You must create /var/spool/zbot directory"
			ewarn "that is writable for the zbot user, or "
			ewarn "specify a different directory in zbot.ini"
			ewarn "permissions should be u=rwx (0750)"
			ewarn
	fi
}
