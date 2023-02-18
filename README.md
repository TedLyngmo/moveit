# mvx
A tool for moving a certain amount of bytes from one directory to another

## Building
`make`

## Installation
Copy `mvx` to `/usr/local/bin` or a similar directory present in your `PATH`.

## Limits

Up to 8 EiB (9223372036854775807 bytes) can be moved per run.

## Usage example
This will move at least 30GB of files. The oldest files are those picked first.
```
mvx source_dir destination_dir 30G
```
