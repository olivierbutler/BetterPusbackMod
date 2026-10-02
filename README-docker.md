# Cross-compile for Linux and Windows in a container

The Windows and Linux plugins are built in a Debian 11 (bullseye) container
that has the MinGW cross-compiler, from macOS (Docker Desktop) or from Linux
(Docker or Podman).

## Folder layout

The container sees the folder *above* this project as `/xpl_dev`, and the
plugin build expects libacfutils (https://github.com/olivierbutler/libacfutils)
next to this project, built for both platforms:

```
# Host folders         | Inside the container
# ---------------------|---------------------
# ../projets/          | /xpl_dev/
# ├─ libacfutils/      | ├── libacfutils/
# └─ BetterPusbackMod/ | └── BetterPusbackMod/
```

`PROJECT_PATH` in `build_xpl.sh` names this project's folder
(`BetterPusbackMod`); change it if yours differs.

## 1. Build the container image

From this folder:

```
docker build -t cross-m-w-l:latest -f Dockerfile.win-lin .
```

`./build_xpl.sh -f` (macOS) runs the build through `docker compose`, which
needs a `docker-compose.yml` in this folder. It is a local file (ignored by
git), since its paths are yours; this one matches the layout above:

```
services:
  win-lin-build:
    image: cross-m-w-l:latest
    container_name: cross-m-w-l
    build:
      context: .
      dockerfile: Dockerfile.win-lin
    volumes:
      - ..:/xpl_dev
```

With it, `docker compose build` builds the image too.

Debian 11 left security support in August 2026, and its live mirrors now list
packages whose files are gone, so the image reads bullseye from
snapshot.debian.org at a fixed date (`DEBIAN_SNAPSHOT` in `Dockerfile.win-lin`).
Use another date with `--build-arg DEBIAN_SNAPSHOT=20260801T000000Z`.

## 2. Build libacfutils (once)

libacfutils has the same Dockerfile; build its dependencies and library in the
container (see libacfutils' own README-docker.md):

```
docker run --rm -v "$PWD/..:/xpl_dev" cross-m-w-l:latest \
    bash -c "cd libacfutils && ./build_deps && ./build_redist"
```

Clone libacfutils with `--recursive`: its build needs the `mingw-std-threads`
submodule. If the submodule's recorded commit can no longer be fetched (as of
October 2026 it cannot), check out `6c2061b` in `libacfutils/mingw-std-threads`
by hand: the newest commit before C++20 `<latch>` support, which this
container's MinGW (GCC 10) does not have.

## 3. Build the plugin

From macOS:

```
./build_xpl.sh -f
```

builds the Mac plugin natively, then the Linux and Windows plugins in the
container (`docker compose run`).

From Linux:

```
docker run --rm -v "$PWD/..:/xpl_dev" cross-m-w-l:latest \
    bash -c "cd BetterPusbackMod && ./build_xpl.sh"
```

The plugins land in `win_x64/` and `lin_x64/` (and together in
`BetterPushback/`). The unit tests run on the host:
`sh tests/run_all_tests.sh` (needs a C compiler and python3).

## Podman

Podman (the default container tool on Fedora and RHEL; rootless, no daemon)
runs the same image. Use `podman` in place of `docker` in the commands above,
with two differences:

- On SELinux systems (Fedora, RHEL) add `:Z` to the volume so the container may
  use the files: `-v "$PWD/..:/xpl_dev:Z"`.
- Rootless containers map users: files the build creates as the container's
  root belong to you, but files unpacked with their archive's owners (some of
  libacfutils' dependencies) belong to a mapped user you cannot delete as.
  Remove those with `podman unshare rm -rf <folder>`.

For example:

```
podman build -t cross-m-w-l:latest -f Dockerfile.win-lin .
podman run --rm -v "$PWD/..:/xpl_dev:Z" cross-m-w-l:latest \
    bash -c "cd BetterPusbackMod && ./build_xpl.sh"
```
