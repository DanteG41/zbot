# Copyright 1999-2024 Gentoo Authors
# Distributed under the terms of the GNU General Public License v2

EAPI=7

inherit cmake

DESCRIPTION=""
HOMEPAGE="https://github.com/DanteG41/zbot"

TGBOT_COMMIT=601fd7fa0e879084c01ebd59741d73c8f8144b88
SIMPLEINI_COMMIT=fe082fa81f4a55ddceb55056622136be616b3c6f

SRC_URI="https://github.com/DanteG41/${PN}/archive/v${PV}.tar.gz -> ${P}.tar.gz
https://github.com/DanteG41/tgbot-cpp/archive/${TGBOT_COMMIT}.tar.gz -> tgbot-cpp-${TGBOT_COMMIT}.tar.gz
https://github.com/brofield/simpleini/archive/${SIMPLEINI_COMMIT}.tar.gz -> simpleini-${SIMPLEINI_COMMIT}.tar.gz
"

SLOT="0"
LICENSE="GPL-3.0"
KEYWORDS="amd64 ~x86"
RESTRICT="mirror"

RDEPEND="
	acct-user/zbot
	acct-group/zbot
	dev-libs/openssl
	dev-libs/boost
	sys-libs/zlib
"

DEPEND="${RDEPEND}"

src_prepare() {
	rmdir libs/tgbot libs/simpleini || die
	mv "${WORKDIR}/tgbot-cpp-${TGBOT_COMMIT}" libs/tgbot || die
	mv "${WORKDIR}/simpleini-${SIMPLEINI_COMMIT}" libs/simpleini || die

	cmake_src_prepare
}

src_configure() {
	local mycmakeargs=(
		-DBUILD_SHARED_LIBS=OFF
	)

	cmake_src_configure
}

src_install() {
	cmake_src_install

	newconfd "${FILESDIR}/zbotd-confd-r1" zbotd
	newinitd "${FILESDIR}/zbotd-initd-r1" zbotd

	insinto	/etc/zbot
	newins misc/sample_config.ini zbot.ini
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
