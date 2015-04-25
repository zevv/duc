# Copyright 1999-2015 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI=5

inherit eutils 

if [[ "${PV}" == "9999" ]]; then
	inherit git-r3
	EGIT_REPO_URI="https://github.com/zevv/duc.git"
else
	KEYWORDS="~amd64"
	SRC_URI="https://github.com/zevv/${PN}/releases/download/${PV}/${P/_rc/-rc}.tar.gz"
fi

DESCRIPTION="A suite of tools for inspecting disk usage"
HOMEPAGE="https://github.com/zevv/duc"
LICENSE="GPL-2"
SLOT="0"
IUSE="ui gui graph"
S="${WORKDIR}/${P/_rc/-rc}"

DEPEND="dev-db/tokyocabinet
	ui? (
		sys-libs/ncurses
	)
	gui? (
		x11-libs/libX11
		x11-libs/cairo[X]
		x11-libs/pango
	)
	graph? (
		x11-libs/cairo
		x11-libs/pango
	)
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
	sed -i -e "/ldconfig/d" -e "/install-exec-hook/d" Makefile.in || die
}

src_configure() {
	econf  \
		$(use_enable ui) \
		$(use_enable gui) \
		$(use_enable graph)
}

