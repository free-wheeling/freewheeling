
the files contained in this directory are used to build/package binary artifacts and are otherwise not interesting

when merging into master, the current manual procedure is as follows:
* in ChangeLog  
  => add new entry with list of significant commits
* update AC_INIT version number in configure.ac then `autoreconf -ivf` and commit
* create a tag/tarball named as "vMAJOR.MINOR.REV" and push upstream
* download the tarball from the repo "release" and calclulate the md5sum
* in packaging/PKGBUILD  
  => update 'pkgver' and 'md5sums' to match the new tarball  
  => update 'makedepends' and/or 'depends'  
  => update 'validpgpkeys'
* `gpg --detach-sign packaging/PKGBUILD` and upload it to the repo "release"
* update main README.md as necessary, git commit everything, then merge into master
