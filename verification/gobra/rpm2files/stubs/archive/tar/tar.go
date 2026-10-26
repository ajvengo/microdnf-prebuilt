//go:build gobra

// Package tar is a TRUSTED Gobra stub of the parts of archive/tar that
// resolveMember touches: two constants and three fields of Header.
package tar

const (
	TypeLink    = '1'
	TypeSymlink = '2'
)

type Header struct {
	Typeflag byte
	Name     string
	Linkname string
}
