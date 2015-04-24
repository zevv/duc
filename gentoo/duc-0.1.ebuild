# Copyright 1999-2015 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI=5

AUTOTOOLS_AUTORECONF="true"
AUTOTOOLS_IN_SOURCE_BUILD="true"
inherit autotools-utils

if [[ "${PV}" == "9999" ]]; then
	inherit git-r3
	EGIT_REPO_URI="https://github.com/zevv/duc.git"
elif [[ "${PV}" == "0.1" ]]; then
	MY_PV="${PV}-test"
	MY_P="${PN}-${MY_PV}"
	KEYWORDS="~amd64"
	SRC_URI="https://github.com/zevv/${PN}/releases/download/${MY_PV}/${MY_P}.tar.gz"
	S="${WORKDIR}/${MY_P}"
else
	KEYWORDS="~amd64"
	SRC_URI="https://github.com/zevv/${PN}/releases/download/${PV}/${P}.tar.gz"
fi

DESCRIPTION="A library and suite of tools for inspecting disk usage"
HOMEPAGE="https://github.com/zevv/duc"

LICENSE="GPL-2"
SLOT="0"
IUSE=""

DEPEND="dev-db/tokyocabinet
	sys-libs/ncurses
	x11-libs/libX11
	x11-libs/cairo[X]
	x11-libs/pango
"
RDEPEND="${DEPEND}"

src_unpack() {
	if [[ "${PV}" == "9999" ]]; then
		git-r3_src_unpack
	else
		unpack "${A}"
	fi
}

src_prepare() {
	sed -i -e "/ldconfig/d" -e "/install-exec-hook/d" Makefile.am || die
	sed -i -e "/bin_PROGRAMS/s/duc.debug//" duc/Makefile.am || die

	autotools-utils_src_prepare
}

src_configure() {
	local myeconfargs=(
		--disable-static
	)

	autotools-utils_src_configure
}

