import subprocess
import os
import shutil

if shutil.which("glslc"):
    root = os.path.abspath("./shaders")
    if(os.path.isdir(root)):
        spv_root = os.path.join(root, "spv")
        source_root = os.path.join(root, "source")

        os.makedirs(spv_root, exist_ok=True)
        for dname in os.listdir(source_root):
            shader_dir = os.path.join(source_root, dname)
            spv_dir = os.path.join(spv_root, dname)
            shutil.rmtree(spv_dir)
            os.makedirs(spv_dir, exist_ok=False)
            for fname in os.listdir(shader_dir):
                shader_file = os.path.join(shader_dir, fname)
                name, ext = os.path.splitext(shader_file)
                if ext not in [".vert", ".frag", ".tesc", ".tese", ".geom", ".comp"]:
                    print(f"Invalid file extension at path {shader_file}")
                    continue
                spv_file = os.path.join(spv_dir, f"{ext[1:]}.spv")
                out = subprocess.run(f"glslc {shader_file} -o {spv_file}",
                                     shell=True, capture_output=True)
                if out.stdout:
                    print(out.stdout)
                if out.stderr:
                    print(out.stderr)
    else:
        print("./shader directory not found at current path")
else:
    print("glslc not found in path")
