# allmuxcpp

## Build With Docker

Build the reusable build image:

```sh
docker build -t allmux-release-build .
```

Build the project inside a temporary container:

```sh
docker run --rm --user "$(id -u):$(id -g)" -v "$PWD:/src" -w /src allmux-release-build sh -c 'rm -rf build/docker-release && cmake -S /src -B /src/build/docker-release -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build /src/build/docker-release'
```

The Docker-built binary is written to `build/docker-release/allmuxcpp`.
