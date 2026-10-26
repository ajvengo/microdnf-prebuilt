//go:build gobra

// Package fmt is a TRUSTED Gobra stub of fmt.Errorf: total, returns a non-nil
// error. Only the shape the verified code needs is declared.
package fmt

//@ trusted
//@ ensures e != nil
//@ decreases
func Errorf(format string, a ...interface{}) (e error)
