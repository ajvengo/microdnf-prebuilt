//go:build gobra

// Extracted from rpm2files/main.go for Gobra. Function bodies are byte-for-byte
// the shipped ones (enforced by ../extract_test.go); only the //@ annotations,
// the tarEntry type and the imports of the stubs differ. See README.md.
package main

import (
	"archive/tar"
	"fmt"
	"path"
	"strings"
)

type tarEntry struct {
	header  *tar.Header
	content []byte
}

//@ ghost
//@ requires acc(m, _)
//@ decreases
//@ pure func at(m map[string]*tarEntry, k string) *tarEntry {
//@ 	return m[k]
//@ }

//@ ensures len(r) == 0 || r[0] != '/'
//@ decreases
func cleanTarPath(p string) (r string) {
	p = path.Clean(p)
	//@ strings.Literals()
	p = strings.TrimPrefix(p, "/")
	//@ assert forall i int :: 0 <= i && i + 1 < len(p) ==> !(p[i] == '/' && p[i+1] == '/')
	//@ assert len(p) == 0 || p[0] != '/'
	//@ ghost s1 := p
	p = strings.TrimPrefix(p, "./")
	//@ assert len(p) == 0 || p[0] != '/'
	return p
}

//@ requires acc(members, 1/2)
//@ requires acc(visited)
//@ requires 0 <= depth && depth <= 65
//@ requires forall k string :: k elem domain(members) ==> at(members, k) != nil
//@ requires forall k1, k2 string :: k1 elem domain(members) && k2 elem domain(members) && k1 != k2 ==> at(members, k1) != at(members, k2)
//@ requires forall k string :: k elem domain(members) ==> acc(at(members, k), 1/2) && at(members, k).header != nil
//@ requires forall k1, k2 string :: k1 elem domain(members) && k2 elem domain(members) && k1 != k2 ==> at(members, k1).header != at(members, k2).header
//@ requires forall k string :: k elem domain(members) ==> acc(at(members, k).header, 1/2)
//@ ensures acc(members, 1/2)
//@ ensures acc(visited)
//@ ensures forall k string :: k elem domain(members) ==> acc(at(members, k), 1/2)
//@ ensures forall k string :: k elem domain(members) ==> acc(at(members, k).header, 1/2)
//@ ensures forall k string :: k elem domain(members) ==> at(members, k) == old(at(members, k))
//@ ensures err == nil ==> entry != nil
//@ ensures len(visited) <= old(len(visited)) + 65 - depth
//@ decreases 65 - depth
func resolveMember(members map[string]*tarEntry, name string, visited map[string]bool, depth int) (entry *tarEntry, err error) {
	if depth > 64 {
		return nil, fmt.Errorf("exceeded maximum symlink depth (64) while resolving %s", name)
	}
	cleanName := cleanTarPath(name)
	if visited[cleanName] {
		return nil, fmt.Errorf("cyclic symlink detected at %s", name)
	}
	visited[cleanName] = true

	entry, ok := members[cleanName]
	if !ok {
		return nil, fmt.Errorf("path %s not found in archive", name)
	}
	//@ assert entry == at(members, cleanName)
	//@ assert entry != nil && entry.header != nil

	if entry.header.Typeflag == tar.TypeSymlink || entry.header.Typeflag == tar.TypeLink {
		target := entry.header.Linkname
		if !strings.HasPrefix(target, "/") {
			target = path.Clean(path.Join(path.Dir(cleanName), target))
		}
		target = cleanTarPath(target)
		return resolveMember(members, target, visited, depth+1)
	}

	return entry, nil
}
