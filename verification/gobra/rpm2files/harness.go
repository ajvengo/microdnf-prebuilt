//go:build gobra

package main

import "archive/tar"

// harness mirrors main() and FuzzResolveMember: a fresh visited map, depth 0, a
// members map whose values are distinct freshly allocated entries. It proves
// the preconditions of resolveMember are satisfiable (not vacuous) and that the
// postcondition is usable by a caller.
func harness(name string) {
	members := make(map[string]*tarEntry)
	e := &tarEntry{header: &tar.Header{Typeflag: tar.TypeSymlink, Linkname: "b"}}
	members["a"] = e
	visited := make(map[string]bool)
	//@ assert "a" elem domain(members)
	//@ assert forall k string :: k elem domain(members) ==> at(members, k) != nil && acc(at(members, k), 1/2) && at(members, k).header != nil && acc(at(members, k).header, 1/2)
	_, _ = resolveMember(members, name, visited, 0)
	//@ assert len(visited) <= 65
}
