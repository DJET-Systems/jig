# Homebrew formula for jig.
# Source of truth is this file; sync it into the tap repository per release:
#   repo: github.com/ansgrb/jig-tap (or homebrew-jig)
#   path: Formula/jig.rb
# After the release workflow publishes SHA256SUMS, paste the hash below.
class Jig < Formula
  desc "The native C/C++ toolchain: pinned compilers, per-project environments"
  homepage "https://github.com/ansgrb/jig"
  url "https://github.com/ansgrb/jig/releases/download/v0.1.0/jig-0.1.0.tar.gz"
  sha256 "PASTE_FROM_SHA256SUMS_AT_RELEASE"
  license "AGPL-3.0-only"

  head "https://github.com/ansgrb/jig.git", branch: "main"

  depends_on "cmake" => :build
  depends_on "ninja" => :build
  # Homebrew compiles with the system Xcode clang; jig needs C++23
  # (Xcode 15 command line tools or newer).

  def install
    system "cmake", "-S", ".", "-B", "build",
           "-DCMAKE_BUILD_TYPE=Release",
           "-DJIG_BUILD_TESTS=OFF",
           *std_cmake_args
    system "cmake", "--build", "build"
    system "cmake", "--install", "build"
  end

  test do
    assert_match "jig", shell_output("#{bin}/jig version")
  end
end
