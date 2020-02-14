# Copyright 1999-2020 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

EAPI=6

inherit cmake-utils git-r3 user

DESCRIPTION=""
HOMEPAGE="https://github.com/DanteG41/zbot"
EGIT_REPO_URI="https://github.com/DanteG41/zbot.git"
EGIT_COMMIT="v${PV}"
EGIT_SUBMODULES=( libs/simpleini libs/tgbot )
CMAKE_MIN_VERSION=3.3.1-r1
LICENSE="GPL-3.0"
SLOT="0"
KEYWORDS="~amd64 ~x86"

RDEPEND="
	dev-libs/openssl
	dev-libs/boost
	sys-libs/zlib
"

DEPEND="${RDEPEND}"

pkg_setup() {
	enewgroup	zbot 80
	enewuser	zbot 80 -1 -1 zbot

}

src_install() {
	cmake-utils_src_install

	newconfd	"${FILESDIR}/zbotd-confd-r1" "zbotd"
	newinitd	"${FILESDIR}/zbotd-initd-r1" "zbotd"

	insinto		/etc/zbot
	newins		misc/sample_config.ini zbot.ini
}

pkg_postinst() {
		ewarn
		ewarn "To be able to send messages via zbotcli"
		ewarn "from a user other than zbot, add"
		ewarn "your user to the zbot group:"
		ewarn "useradd -a -G zbot your_user"
		ewarn
	if [[ ! -d /var/spool/zbot ]]; then
			ewarn
			ewarn "You must create /var/spool/zbot directory"
			ewarn "that is writable for the zbot user, or "
			ewarn "specify a different directory in zbot.ini"
			ewarn "permissions should be u=rwx g=rws (2770)"
			ewarn
	fi
}
