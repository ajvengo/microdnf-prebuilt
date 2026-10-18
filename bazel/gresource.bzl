"""Custom Starlark rule to compile GLib GResource XML descriptors into C source files."""

def _glib_compile_resources_impl(ctx):
    out = ctx.actions.declare_file(ctx.attr.out)
    xml_file = ctx.file.xml
    args = ctx.actions.args()
    args.add("--generate-source")
    args.add("--sourcedir=" + xml_file.dirname)
    args.add("--target=" + out.path)
    if ctx.attr.internal:
        args.add("--internal")
    if ctx.attr.c_prefix:
        args.add("--c-name=" + ctx.attr.c_prefix)
    args.add(xml_file.path)

    ctx.actions.run(
        executable = ctx.executable._compiler,
        arguments = [args],
        inputs = [xml_file] + ctx.files.resources,
        outputs = [out],
        mnemonic = "GlibCompileResources",
        progress_message = "Compiling GResource %s" % xml_file.short_path,
    )
    return [DefaultInfo(files = depset([out]))]

glib_compile_resources = rule(
    implementation = _glib_compile_resources_impl,
    attrs = {
        "c_prefix": attr.string(mandatory = True),
        "internal": attr.bool(default = True),
        "out": attr.string(mandatory = True),
        "resources": attr.label_list(mandatory = True, allow_files = True),
        "xml": attr.label(mandatory = True, allow_single_file = [".xml"]),
        "_compiler": attr.label(
            default = Label("@glib//:glib-compile-resources"),
            executable = True,
            cfg = "exec",
        ),
    },
)
