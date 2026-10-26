//go:build gobra

// Package strings is a TRUSTED Gobra stub of the two strings functions the
// verified code calls. Contracts restate the documented behaviour
// (https://pkg.go.dev/strings#TrimPrefix, #HasPrefix) in byte-index form and
// add termination.
package strings

//@ ghost
//@ decreases
//@ pure func hasPrefix(s, prefix string) bool {
//@ 	return len(prefix) <= len(s) && forall j int :: 0 <= j && j < len(prefix) ==> s[j] == prefix[j]
//@ }

// HasPrefix reports whether s begins with prefix.
//@ trusted
//@ pure
//@ ensures r == hasPrefix(s, prefix)
//@ decreases
func HasPrefix(s, prefix string) (r bool)

// TrimPrefix returns s without the provided leading prefix string; if s does
// not start with prefix, s is returned unchanged.
//@ trusted
//@ ensures hasPrefix(s, prefix) ==> len(r) == len(s) - len(prefix)
//@ ensures hasPrefix(s, prefix) ==> forall i int :: 0 <= i && i < len(r) ==> r[i] == s[i + len(prefix)]
//@ ensures !hasPrefix(s, prefix) ==> r == s
//@ decreases
func TrimPrefix(s, prefix string) (r string)

// Literals states the byte content of the two string literals cleanTarPath
// passes to TrimPrefix; Gobra does not model string-literal bytes itself.
//@ ghost
//@ trusted
//@ ensures len("/") == 1 && "/"[0] == '/'
//@ ensures len("./") == 2 && "./"[0] == '.' && "./"[1] == '/'
//@ decreases
//@ func Literals()
