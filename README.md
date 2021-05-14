# Dump FPV live video feed to stdout, written in C

## Installation
You can download the latest release [here](https://github.com/fpvout/fpv-c/releases/tag/latest), or build it yourself by following what CI/CD does [here](https://github.com/fpvout/fpv-c/blob/master/.github/workflows/c-cpp.yml)/

## Usage

```
./fpv-video-out | ffplay -i - -analyzeduration 1 -probesize 32 -sync ext
```
