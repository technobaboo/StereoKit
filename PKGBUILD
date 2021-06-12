pkgname=stereokit
pkgver=0.3.1
pkgrel=1

pkgdesc="Open source library for building cross-platform XR apps with C# C++ and OpenXR"
arch=('x86_64' 'aarch64')
url="https://stereokit.net/"
install -Dm644 LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
license=('MIT')

depends=('openxr>=1.0.16' 'fontconfig')
makedepends=('xmake>=2.5.3')

source=(https://github.com/maluoi/StereoKit/archive/refs/tags/v$pkgver.tar.gz)
md5sums=('d39b9ad590ead104221f02122da754eb')

build() {
	cd "$srcdir/StereoKit-$pkgver"
	xmake f -k shared
	xmake build
}

install() {
	cd "$srcdir/StereoKit-$pkgver"
	xmake install -o /usr --admin
}

package() {
	cd "$srcdir/StereoKit-$pkgver"
	xmake install -o $pkgdir/ --admin
}