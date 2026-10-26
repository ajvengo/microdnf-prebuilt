package main

import (
	"archive/tar"
	"bytes"
	"compress/gzip"
	"io"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/klauspost/compress/zstd"
)

const (
	memberName    = "etc/hello"
	memberContent = "hello zstd\n"
)

func plainTar(t *testing.T) []byte {
	t.Helper()
	var buf bytes.Buffer
	tw := tar.NewWriter(&buf)
	if err := tw.WriteHeader(&tar.Header{Name: memberName, Mode: 0o644, Size: int64(len(memberContent)), Typeflag: tar.TypeReg}); err != nil {
		t.Fatal(err)
	}
	if _, err := tw.Write([]byte(memberContent)); err != nil {
		t.Fatal(err)
	}
	if err := tw.Close(); err != nil {
		t.Fatal(err)
	}
	return buf.Bytes()
}

func gzipBytes(t *testing.T, in []byte) []byte {
	t.Helper()
	var buf bytes.Buffer
	zw := gzip.NewWriter(&buf)
	if _, err := zw.Write(in); err != nil {
		t.Fatal(err)
	}
	if err := zw.Close(); err != nil {
		t.Fatal(err)
	}
	return buf.Bytes()
}

func zstdBytes(t *testing.T, in []byte) []byte {
	t.Helper()
	var buf bytes.Buffer
	zw, err := zstd.NewWriter(&buf)
	if err != nil {
		t.Fatal(err)
	}
	if _, err := zw.Write(in); err != nil {
		t.Fatal(err)
	}
	if err := zw.Close(); err != nil {
		t.Fatal(err)
	}
	return buf.Bytes()
}

func TestOpenArchive(t *testing.T) {
	t.Parallel()
	tarData := plainTar(t)
	tests := []struct {
		name string
		data []byte
	}{
		{"plain", tarData},
		{"gzip", gzipBytes(t, tarData)},
		{"zstd", zstdBytes(t, tarData)},
	}
	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			t.Parallel()
			p := filepath.Join(t.TempDir(), "archive")
			if err := os.WriteFile(p, tc.data, 0o644); err != nil {
				t.Fatal(err)
			}
			rc, err := openArchive(p)
			if err != nil {
				t.Fatalf("openArchive: %v", err)
			}
			defer func() { _ = rc.Close() }()
			tr := tar.NewReader(rc)
			hdr, err := tr.Next()
			if err != nil {
				t.Fatalf("tar next: %v", err)
			}
			if hdr.Name != memberName {
				t.Errorf("member name = %q, want %q", hdr.Name, memberName)
			}
			got, err := io.ReadAll(tr)
			if err != nil {
				t.Fatal(err)
			}
			if string(got) != memberContent {
				t.Errorf("content = %q, want %q", got, memberContent)
			}
		})
	}
}

func TestOpenArchiveMissing(t *testing.T) {
	t.Parallel()
	if _, err := openArchive(filepath.Join(t.TempDir(), "nope")); err == nil {
		t.Fatal("expected error for missing archive")
	}
}

// cleanTarPathSeeds are the shapes that matter for tar member names: absolute,
// dot-prefixed, doubled separators, parent traversal, the root and the empty name.
var cleanTarPathSeeds = []string{
	"", ".", "/", "//", "/.", "/..", "./", "./a", "/a", "//a", "/./a", "a/../b",
	"../a", "/../a", "a//b/./c/", "etc/hello", "./usr/bin/../lib",
}

func FuzzCleanTarPath(f *testing.F) {
	for _, s := range cleanTarPathSeeds {
		f.Add(s)
	}
	f.Fuzz(func(t *testing.T, p string) {
		got := cleanTarPath(p)
		if strings.HasPrefix(got, "/") {
			t.Fatalf("cleanTarPath(%q) = %q starts with /", p, got)
		}
		if strings.HasPrefix(got, "./") {
			t.Fatalf("cleanTarPath(%q) = %q starts with ./", p, got)
		}
		// Idempotent on everything except the root, which is "" after one pass
		// and "." after two (see TestCleanTarPathRoot).
		if got != "" {
			if again := cleanTarPath(got); again != got {
				t.Fatalf("cleanTarPath not idempotent: %q -> %q -> %q", p, got, again)
			}
		}
	})
}

// TestCleanTarPathRoot pins the one non-idempotent case found by FuzzCleanTarPath:
// every spelling of the absolute root maps to "" on the first pass and "." on the
// second. Both denote a directory, never a file member, so extraction is unaffected.
func TestCleanTarPathRoot(t *testing.T) {
	t.Parallel()
	for _, p := range []string{"/", "//", "/.", "/..", "/./", "/../.."} {
		if got := cleanTarPath(p); got != "" {
			t.Errorf("cleanTarPath(%q) = %q, want empty", p, got)
		}
	}
	if got := cleanTarPath(""); got != "." {
		t.Errorf("cleanTarPath(\"\") = %q, want %q", got, ".")
	}
	if got := cleanTarPath(cleanTarPath("/")); got != "." {
		t.Errorf("second pass over the root = %q, want %q", got, ".")
	}
}

// buildMembers decodes fuzz bytes into a member map. Lines are
// "<type><name>\t<linkname>"; type 's' is a symlink, 'l' a hardlink, 'd' a
// directory, anything else a regular file.
func buildMembers(data string) map[string]*tarEntry {
	members := map[string]*tarEntry{}
	for _, line := range strings.Split(data, "\n") {
		if line == "" {
			continue
		}
		kind, rest := line[0], line[1:]
		name, link, _ := strings.Cut(rest, "\t")
		flag := byte(tar.TypeReg)
		switch kind {
		case 's':
			flag = tar.TypeSymlink
		case 'l':
			flag = tar.TypeLink
		case 'd':
			flag = tar.TypeDir
		}
		members[cleanTarPath(name)] = &tarEntry{header: &tar.Header{Name: name, Linkname: link, Typeflag: flag}}
	}
	return members
}

func FuzzResolveMember(f *testing.F) {
	f.Add("sa\tb\nsb\ta\n", "a")                 // two-link cycle
	f.Add("sa\ta\n", "a")                        // self link
	f.Add("setc/l\t../usr/x\nfusr/x\n", "etc/l") // relative symlink
	f.Add("la\t/b\nfb\n", "./a")                 // absolute hardlink
	f.Add("sa\t/\nd/\n", "a")                    // link to the root
	f.Add("", "missing")
	var chain strings.Builder
	for i := range 70 { // longer than the depth bound
		chain.WriteString("sl" + string(rune('A'+i%26)) + string(rune('a'+i/26)) + "\t")
		chain.WriteString("l" + string(rune('A'+(i+1)%26)) + string(rune('a'+(i+1)/26)) + "\n")
	}
	f.Add(chain.String(), "lAa")
	f.Fuzz(func(t *testing.T, data, start string) {
		members := buildMembers(data)
		visited := map[string]bool{}
		entry, err := resolveMember(members, start, visited, 0)
		if len(visited) > 65 {
			t.Fatalf("visited %d members, depth bound is 64", len(visited))
		}
		if err == nil {
			if entry == nil {
				t.Fatal("nil entry without error")
			}
			if tf := entry.header.Typeflag; tf == tar.TypeSymlink || tf == tar.TypeLink {
				t.Fatalf("resolved to a link entry %q", entry.header.Name)
			}
		}
		if _, err := resolveMember(members, start, map[string]bool{}, 65); err == nil {
			t.Fatal("depth 65 must be rejected")
		}
	})
}

func FuzzParseBool(f *testing.F) {
	for _, s := range []string{"", "1", "true", " YES ", "y", "0", "no", "\x00"} {
		f.Add(s)
	}
	f.Fuzz(func(t *testing.T, s string) {
		if !parseBool(s) {
			return
		}
		switch strings.ToLower(strings.TrimSpace(s)) {
		case "1", "true", "yes", "y":
		default:
			t.Fatalf("parseBool(%q) = true outside the documented spellings", s)
		}
	})
}

func FuzzParseArgsFile(f *testing.F) {
	f.Add("a b true\n# comment\n\nc d 0\n")
	f.Add("a b\n")
	f.Add("a b c d\n")
	f.Add("  a\tb\tyes  \r\n")
	f.Fuzz(func(t *testing.T, content string) {
		p := filepath.Join(t.TempDir(), "args")
		if err := os.WriteFile(p, []byte(content), 0o644); err != nil {
			t.Fatal(err)
		}
		specs, err := parseArgsFile(p)
		if err != nil && specs != nil {
			t.Fatalf("specs returned alongside error: %v", err)
		}
		for _, s := range specs {
			if s.reqPath == "" || s.outPath == "" || strings.ContainsAny(s.reqPath+s.outPath, " \t\n") {
				t.Fatalf("malformed spec %+v", s)
			}
		}
	})
}
