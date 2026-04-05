import os
import sys
import json
import math
import struct
import base64
from typing import List, Dict, Any, Tuple

COMPONENT_SIZE = {
    5120: 1,  # BYTE
    5121: 1,  # UNSIGNED_BYTE
    5122: 2,  # SHORT
    5123: 2,  # UNSIGNED_SHORT
    5125: 4,  # UNSIGNED_INT
    5126: 4,  # FLOAT
}

TYPE_COUNT = {
    "SCALAR": 1,
    "VEC2": 2,
    "VEC3": 3,
    "VEC4": 4,
}

MODE_TRIANGLES = 4


class GLTF:
    def __init__(self, path: str):
        self.path = path
        self.dir = os.path.dirname(path)

        if path.lower().endswith(".glb"):
            self._load_glb(path)
        else:
            self._load_gltf(path)

    def _load_glb(self, path: str) -> None:
        with open(path, "rb") as f:
            data = f.read()

        if len(data) < 20:
            raise RuntimeError(f"{path}: invalid glb")

        magic, version, length = struct.unpack_from("<III", data, 0)
        if magic != 0x46546C67:
            raise RuntimeError(f"{path}: invalid glb magic")
        if version != 2:
            raise RuntimeError(f"{path}: unsupported glb version {version}")
        if length > len(data):
            raise RuntimeError(f"{path}: invalid glb length")

        offset = 12

        json_chunk_length, json_chunk_type = struct.unpack_from("<II", data, offset)
        offset += 8
        if json_chunk_type != 0x4E4F534A:
            raise RuntimeError(f"{path}: first chunk is not JSON")

        json_bytes = data[offset:offset + json_chunk_length]
        offset += json_chunk_length
        self.json = json.loads(json_bytes.decode("utf-8"))

        self.binary = b""
        while offset + 8 <= len(data):
            chunk_length, chunk_type = struct.unpack_from("<II", data, offset)
            offset += 8
            chunk_data = data[offset:offset + chunk_length]
            offset += chunk_length

            if chunk_type == 0x004E4942:
                self.binary = chunk_data
                break

    def _load_gltf(self, path: str) -> None:
        with open(path, "r", encoding="utf-8") as f:
            self.json = json.load(f)

        if not self.json.get("buffers"):
            self.binary = b""
            return

        uri = self.json["buffers"][0].get("uri", "")
        if uri.startswith("data:"):
            comma = uri.find(",")
            if comma < 0:
                raise RuntimeError(f"{path}: invalid data uri")
            self.binary = base64.b64decode(uri[comma + 1:])
        else:
            bin_path = os.path.join(self.dir, uri)
            with open(bin_path, "rb") as f:
                self.binary = f.read()

    def accessor_info(self, accessor_index: int) -> Dict[str, Any]:
        acc = self.json["accessors"][accessor_index]

        if "sparse" in acc:
            raise RuntimeError(f"{self.path}: sparse accessors are not supported")

        view_index = acc.get("bufferView")
        if view_index is None:
            raise RuntimeError(f"{self.path}: accessor without bufferView is not supported")

        view = self.json["bufferViews"][view_index]

        comp_type = acc["componentType"]
        elems = TYPE_COUNT[acc["type"]]
        comp_size = COMPONENT_SIZE[comp_type]

        stride = view.get("byteStride", comp_size * elems)
        base_offset = view.get("byteOffset", 0) + acc.get("byteOffset", 0)

        return {
            "offset": base_offset,
            "count": acc["count"],
            "component_type": comp_type,
            "elem_count": elems,
            "stride": stride,
            "normalized": acc.get("normalized", False),
        }


def read_component(binary: bytes, comp_type: int, pos: int):
    if comp_type == 5120:
        return struct.unpack_from("<b", binary, pos)[0]
    if comp_type == 5121:
        return struct.unpack_from("<B", binary, pos)[0]
    if comp_type == 5122:
        return struct.unpack_from("<h", binary, pos)[0]
    if comp_type == 5123:
        return struct.unpack_from("<H", binary, pos)[0]
    if comp_type == 5125:
        return struct.unpack_from("<I", binary, pos)[0]
    if comp_type == 5126:
        return struct.unpack_from("<f", binary, pos)[0]
    raise RuntimeError(f"unsupported component type: {comp_type}")


def normalize_integer_value(value, comp_type: int) -> float:
    if comp_type == 5121:
        return value / 255.0
    if comp_type == 5123:
        return value / 65535.0
    if comp_type == 5120:
        return max(value / 127.0, -1.0)
    if comp_type == 5122:
        return max(value / 32767.0, -1.0)
    return float(value)


def read_accessor(gltf: GLTF, accessor_index: int) -> List[List[float]]:
    info = gltf.accessor_info(accessor_index)

    offset = info["offset"]
    count = info["count"]
    comp_type = info["component_type"]
    elem_count = info["elem_count"]
    stride = info["stride"]
    normalized = info["normalized"]
    comp_size = COMPONENT_SIZE[comp_type]

    out = []
    for i in range(count):
        base = offset + i * stride
        values = []
        for j in range(elem_count):
            pos = base + j * comp_size
            v = read_component(gltf.binary, comp_type, pos)
            if normalized and comp_type != 5126:
                v = normalize_integer_value(v, comp_type)
            values.append(v)
        out.append(values)

    return out


def pad4_float(values, default_w: float = 0.0) -> List[float]:
    out = [0.0, 0.0, 0.0, default_w]
    for i in range(min(len(values), 4)):
        out[i] = float(values[i])
    return out


def pad4_u32(values) -> List[int]:
    out = [0, 0, 0, 0]
    for i in range(min(len(values), 4)):
        out[i] = int(values[i])
    return out


def normalize_weights4(values: List[float]) -> List[float]:
    s = values[0] + values[1] + values[2] + values[3]
    if s > 0.0:
        return [values[0] / s, values[1] / s, values[2] / s, values[3] / s]
    return [1.0, 0.0, 0.0, 0.0]


def to_macro(name: str) -> str:
    chars = []
    for c in name:
        if c.isalnum():
            chars.append(c.upper())
        else:
            chars.append("_")
    macro = "".join(chars)
    while "__" in macro:
        macro = macro.replace("__", "_")
    return macro.strip("_")


def compute_bounding_sphere_radius_from_vertices(vertex_tuples: List[Tuple]) -> float:
    radius_sq = 0.0
    for item in vertex_tuples:
        p = item[0]
        x = p[0]
        y = p[1]
        z = p[2]
        d2 = x * x + y * y + z * z
        if d2 > radius_sq:
            radius_sq = d2
    return math.sqrt(radius_sq)


def make_vertex_key(p: List[float], n: List[float], t: List[float], j: List[int], w: List[float]):
    return (
        p[0], p[1], p[2], p[3],
        n[0], n[1], n[2], n[3],
        t[0], t[1], t[2], t[3],
        j[0], j[1], j[2], j[3],
        w[0], w[1], w[2], w[3],
    )


def gather_primitive_vertices(gltf: GLTF, prim: Dict[str, Any], file_name: str, mesh_index: int, prim_index: int):
    attrs = prim.get("attributes", {})
    if "POSITION" not in attrs:
        raise RuntimeError(
            f"{file_name}: mesh {mesh_index} primitive {prim_index} has no POSITION"
        )

    positions = read_accessor(gltf, attrs["POSITION"])
    vertex_count = len(positions)

    if "NORMAL" in attrs:
        normals = read_accessor(gltf, attrs["NORMAL"])
    else:
        normals = [[0.0, 0.0, 1.0] for _ in range(vertex_count)]

    if "TEXCOORD_0" in attrs:
        texcoords = read_accessor(gltf, attrs["TEXCOORD_0"])
    else:
        texcoords = [[0.0, 0.0] for _ in range(vertex_count)]

    if "JOINTS_0" in attrs:
        joints = read_accessor(gltf, attrs["JOINTS_0"])
    else:
        joints = [[0, 0, 0, 0] for _ in range(vertex_count)]

    if "WEIGHTS_0" in attrs:
        weights = read_accessor(gltf, attrs["WEIGHTS_0"])
    else:
        weights = [[1.0, 0.0, 0.0, 0.0] for _ in range(vertex_count)]

    if not (
        len(normals) == vertex_count
        and len(texcoords) == vertex_count
        and len(joints) == vertex_count
        and len(weights) == vertex_count
    ):
        raise RuntimeError(
            f"{file_name}: attribute counts do not match in mesh {mesh_index} primitive {prim_index}"
        )

    vertices = []
    for i in range(vertex_count):
        p = pad4_float(positions[i], 1.0)
        n = pad4_float(normals[i], 0.0)
        t = pad4_float(texcoords[i], 0.0)
        j = pad4_u32(joints[i])
        w = normalize_weights4(pad4_float(weights[i], 0.0))
        vertices.append((p, n, t, j, w))

    if "indices" in prim:
        raw_indices = read_accessor(gltf, prim["indices"])
        indices = [int(x[0]) for x in raw_indices]
    else:
        indices = list(range(vertex_count))

    return vertices, indices


def merge_mesh_primitives(gltf: GLTF, mesh: Dict[str, Any], file_name: str, mesh_index: int):
    primitives = mesh.get("primitives", [])
    merged_vertices: List[Tuple] = []
    merged_indices: List[int] = []
    unique_map: Dict[Tuple, int] = {}

    for prim_index, prim in enumerate(primitives):
        mode = prim.get("mode", MODE_TRIANGLES)
        if mode != MODE_TRIANGLES:
            raise RuntimeError(
                f"{file_name}: mesh {mesh_index} primitive {prim_index} is not TRIANGLES"
            )

        prim_vertices, prim_indices = gather_primitive_vertices(gltf, prim, file_name, mesh_index, prim_index)

        remap: List[int] = [0] * len(prim_vertices)
        for local_index, vertex in enumerate(prim_vertices):
            key = make_vertex_key(*vertex)
            merged_index = unique_map.get(key)
            if merged_index is None:
                merged_index = len(merged_vertices)
                unique_map[key] = merged_index
                merged_vertices.append(vertex)
            remap[local_index] = merged_index

        for idx in prim_indices:
            if idx < 0 or idx >= len(remap):
                raise RuntimeError(
                    f"{file_name}: mesh {mesh_index} primitive {prim_index} has invalid index {idx}"
                )
            merged_indices.append(remap[idx])

    if len(merged_vertices) > 65535:
        raise RuntimeError(
            f"{file_name}: mesh {mesh_index} has {len(merged_vertices)} merged vertices, exceeds u16 index limit"
        )

    for idx in merged_indices:
        if idx < 0 or idx > 65535:
            raise RuntimeError(
                f"{file_name}: mesh {mesh_index} merged index outside u16 range"
            )

    return merged_vertices, merged_indices


def build_vertex_blob(vertices: List[Tuple]) -> bytes:
    parts = []
    for p, n, t, j, w in vertices:
        parts.append(
            struct.pack(
                "<4f4f4f4I4f",
                p[0], p[1], p[2], p[3],
                n[0], n[1], n[2], n[3],
                t[0], t[1], t[2], t[3],
                j[0], j[1], j[2], j[3],
                w[0], w[1], w[2], w[3],
            )
        )
    return b"".join(parts)


def main() -> None:
    if len(sys.argv) != 4:
        print("usage: py models_join.py <dir> <bin> <header>")
        sys.exit(1)

    src_dir = sys.argv[1]
    bin_path = sys.argv[2]
    hdr_path = sys.argv[3]

    if not os.path.isdir(src_dir):
        raise RuntimeError(f"directory not found: {src_dir}")

    files = []
    for name in os.listdir(src_dir):
        lower = name.lower()
        if lower.endswith(".gltf") or lower.endswith(".glb"):
            files.append(name)
    files.sort()

    offset = 0
    header = []
    header.append("#ifndef _MODELS_AUTO_INCLUDED")
    header.append("#define _MODELS_AUTO_INCLUDED")
    header.append("")

    with open(bin_path, "wb") as out:
        for file_name in files:
            full_path = os.path.join(src_dir, file_name)
            gltf = GLTF(full_path)

            base_macro = to_macro(os.path.splitext(file_name)[0])
            meshes = gltf.json.get("meshes", [])

            for mesh_index, mesh in enumerate(meshes):
                mesh_name = mesh.get("name", f"MESH{mesh_index}")
                mesh_macro = to_macro(mesh_name)
                name = f"{base_macro}_{mesh_macro}"

                merged_vertices, merged_indices = merge_mesh_primitives(gltf, mesh, file_name, mesh_index)
                bounding_radius = compute_bounding_sphere_radius_from_vertices(merged_vertices)

                vertex_blob = build_vertex_blob(merged_vertices)
                index_blob = struct.pack("<" + ("H" * len(merged_indices)), *merged_indices)

                vertices_offset = offset
                vertices_size = len(vertex_blob)
                vertices_count = len(merged_vertices)
                out.write(vertex_blob)
                offset += vertices_size

                indices_offset = offset
                indices_size = len(index_blob)
                indices_count = len(merged_indices)
                out.write(index_blob)
                offset += indices_size

                header.append(f"#define {name}_VERTICES_OFFSET {vertices_offset}")
                header.append(f"#define {name}_VERTICES_SIZE {vertices_size}")
                header.append(f"#define {name}_VERTICES_COUNT {vertices_count}")
                header.append("")
                header.append(f"#define {name}_INDICES_OFFSET {indices_offset}")
                header.append(f"#define {name}_INDICES_SIZE {indices_size}")
                header.append(f"#define {name}_INDICES_COUNT {indices_count}")
                header.append("")
                header.append(f"#define {name}_BOUNDING_SPHERE_RADIUS {bounding_radius:.9f}f")
                header.append("")

    header.append("#endif")

    with open(hdr_path, "w", encoding="utf-8") as f:
        f.write("\n".join(header))


if __name__ == "__main__":
    main()
