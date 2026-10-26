package gobra

import (
	"bytes"
	"go/ast"
	"go/parser"
	"go/printer"
	"go/token"
	"testing"
)

// verified lists the shipped functions whose Gobra-annotated copies live in
// rpm2files/main.go. A body that drifts from the shipped one means the proof
// no longer talks about the code that ships.
var verified = []struct{ src, extracted, decl string }{
	{"../../rpm2files/main.go", "rpm2files/main.go", "cleanTarPath"},
	{"../../rpm2files/main.go", "rpm2files/main.go", "resolveMember"},
	{"../../rpm2files/main.go", "rpm2files/main.go", "tarEntry"},
}

// render prints the named top-level declaration without comments, so the //@
// annotations (comments) and doc comments do not take part in the comparison.
func render(t *testing.T, file, name string) string {
	t.Helper()
	fset := token.NewFileSet()
	f, err := parser.ParseFile(fset, file, nil, 0) // mode 0: comments are dropped
	if err != nil {
		t.Fatalf("parse %s: %v", file, err)
	}
	for _, d := range f.Decls {
		switch d := d.(type) {
		case *ast.FuncDecl:
			if d.Name.Name == name {
				return print(t, fset, &ast.FuncType{Params: d.Type.Params}) + squash(print(t, fset, d.Body))
			}
		case *ast.GenDecl:
			for _, s := range d.Specs {
				if ts, ok := s.(*ast.TypeSpec); ok && ts.Name.Name == name {
					return squash(print(t, fset, ts))
				}
			}
		}
	}
	t.Fatalf("%s: declaration %s not found", file, name)
	return ""
}

func print(t *testing.T, fset *token.FileSet, n ast.Node) string {
	t.Helper()
	var buf bytes.Buffer
	if err := printer.Fprint(&buf, fset, n); err != nil {
		t.Fatal(err)
	}
	return buf.String()
}

func TestExtractedMatchesShipped(t *testing.T) {
	t.Parallel()
	for _, v := range verified {
		t.Run(v.decl, func(t *testing.T) {
			t.Parallel()
			src, ext := render(t, v.src, v.decl), render(t, v.extracted, v.decl)
			// The extracted signature names the result (`(r string)`, `(entry, err)`) so
			// postconditions can mention it; everything after the signature must be equal.
			if src != ext {
				t.Errorf("%s drifted from %s\n--- shipped\n%s\n--- extracted\n%s", v.decl, v.src, src, ext)
			}
		})
	}
}

// squash drops the blank lines the printer keeps where //@ comments were.
func squash(s string) string {
	return string(bytes.ReplaceAll([]byte(s), []byte("\n\n"), []byte("\n")))
}
