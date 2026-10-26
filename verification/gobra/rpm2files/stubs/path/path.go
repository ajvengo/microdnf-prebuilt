//go:build gobra

// Package path is a TRUSTED Gobra stub of the standard library package path.
// Each contract restates documented behaviour of path.Clean / path.Dir /
// path.Join (https://pkg.go.dev/path); see ../../TRUSTED.md.
package path

// Clean: "If the result of this process is an empty string, Clean returns the
// string "."" and "Replace multiple slashes with a single slash".
//@ trusted
//@ ensures len(r) >= 1
//@ ensures forall i int :: 0 <= i && i + 1 < len(r) ==> !(r[i] == '/' && r[i+1] == '/')
//@ decreases
func Clean(p string) (r string)

// Dir: the result is Clean-ed, so it is never empty ("." for an empty path).
//@ trusted
//@ ensures len(r) >= 1
//@ decreases
func Dir(p string) (r string)

// Join: only totality is relied upon (no contract on r).
//@ trusted
//@ decreases
func Join(parts ...string) (r string)
