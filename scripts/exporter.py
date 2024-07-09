

# nodes
#   name of node
#   children

class Node:
    def __init__(self, name, index, parent, inv_bind):
        ...
        self.name = name
        self.index = index
        self.parent = parent
        self.inv_bind = inv_bind

nodes = {}
indices = []
skinned_vertices = []
def format(x):
    return float(round(x, 6))

class SkinnedVertex:
    def __init__(self, position, normal, uv, indices, weights):
        self.position   = position
        self.uv         = uv
        self.normal     = normal
        self.indices    = indices
        self.weights    = weights
    def print_me(self, f):
        f.write(f"{format(self.position[0])} {format(self.position[1])} {format(self.position[2])}\n")
        f.write(f"{format(self.uv[0])} {format(self.uv[1])}\n")
        f.write(f"{format(self.normal[0])} {format(self.normal[1])} {format(self.normal[2])}\n")
        f.write(f"{len(self.indices)}\n")
        for ind in self.indices:
                    f.write(f"{ind} ")
        f.write("\n")
        for weight in self.weights:
                    f.write(f"{format(weight)} ")
        f.write("\n")


        
import bpy
import bmesh

D = bpy.data

bpy.context.view_layer.update()

arm = D.armatures.items()

print("---")
arm = arm[0][1]
for i, bone in enumerate(arm.bones):
    idx = -1
    if bone.parent:
        idx = nodes[bone.parent.name].index
    nodes[bone.name] = Node(bone.name, i, idx, bone.matrix_local.inverted_safe())
    #print(f"{i}: {bone.name}")
    #print(bone.matrix_local)

for node in nodes.values():
#    print(f"{node.name}: {[i for i in node.children]}")
    ...   



mutant_mesh = D.meshes[D.meshes.find("MutantMesh")]

vertices = mutant_mesh.vertices
loops = mutant_mesh.loops
uv = mutant_mesh.uv_layers


verts = {}
norms = {}
uvs = {}

vg = D.objects[1].vertex_groups
vertex_groups = {}
for v in vg:
    vertex_groups[v.index] = v.name


bm = bmesh.new()
bm.from_mesh(mutant_mesh)
bmesh.ops.triangulate(bm, faces=bm.faces)
bm.to_mesh(mutant_mesh)
bm.free()
mutant_mesh.update(calc_edges=False, calc_edges_loose=False)

for fi in range(len(mutant_mesh.polygons)):
    f = mutant_mesh.polygons[fi]
    for lt in range(f.loop_total):
        loop_index = f.loop_start + lt
        ml = mutant_mesh.loops[loop_index]
        mv = mutant_mesh.vertices[ml.vertex_index]
        
        uv = None
        for xt in mutant_mesh.uv_layers:
            if uv is not None:
                continue
            uv = xt.data[loop_index].uv
            
        
        ind = []
        w = []
        
        for vg in mv.groups:
            ind.append(nodes[vertex_groups[vg.group]].index)
            w.append(vg.weight)
        indices.append(len(skinned_vertices))
        skinned_vertices.append(
            SkinnedVertex(
                mv.co,
                ml.normal,
                uv,
                ind,
                w
            )
        )




def output_obj():
    f = open("test.obj", "w")
    f.write("o test\n")
    vertices = {}
    normals = {}
    uvs2 = {}
    for i, (k, v) in enumerate(verts.items()):
        f.write("v")
        f.write(f" {v[0]:12.6f} {v[1]:12.6f} {v[2]:12.6f}\n")
        vertices[k] = i
    for i,(k, v) in enumerate(norms.items()):
        f.write("vn")
        f.write(f" {v[0]:12.6f} {v[1]:12.6f} {v[2]:12.6f}\n")
        normals[k] = i
    for i, (k, v) in enumerate(uvs.items()):
        f.write("vt")
        f.write(f" {v[0]:12.6f} {v[1]:12.6f}\n")
        uvs2[k] = i
    for i in range(0, len(indices), 3):
        f.write("f ")
        for j in range(3):
            f.write(f"{vertices[indices[i + j]] + 1}")
            f.write(f"/{normals[indices[i + j]] + 1}")
            f.write(f"/{uvs2[indices[i + j]] + 1} ")
        f.write("\n")





S = bpy.context.scene

class Animation():
    def __init__(self, name, duration, step_count, xform_cache):
        self.name = name
        self.duration = duration
        self.step_count = step_count
        self.xform_cache = xform_cache


animations = []
print([i.name for i in D.actions])
for action in D.actions:

    start = int(action.frame_range[0])
    end = int(action.frame_range[1] + 0.5)

    frame_len = 1.0 / S.render.fps
    frame_sub  = 0
    if start > 0:
        frame_sub = start * frame_len
    print(start, end, frame_len, frame_sub)

    bpy.context.selected_objects[0].animation_data.action = action
    xform_cache = {}
    for t in range(start, end + 1):
        S.frame_set(t)
        key = t * frame_len - frame_sub     

        for node in bpy.context.scene.objects:
            if node.type == "ARMATURE":
                for bone in node.pose.bones:
                    if bone.name not in xform_cache:
                        xform_cache[bone.name] = []
                    
                    posebone = node.pose.bones[bone.name]
                    parent_posebone = None
                    
                    mtx = posebone.matrix.copy()
                    
                    if bone.parent:
                        parent_posebone = node.pose.bones[bone.parent.name]
                    
                        mtx = parent_posebone.matrix.inverted_safe() @ mtx
                    xform_cache[bone.name].append((key, mtx))
    animations.append(
        Animation(action.name, end * frame_len - frame_sub, end, xform_cache)
    )        

def output_format():
    f = open("test.anim", "w")
    # Node name parent idx
    f.write(f"{len(nodes.values())}\n")
    for node in nodes.values():
        f.write(f"{node.name} {node.parent}\n")
        for i in node.inv_bind:
            for j in i:                
                f.write(f"{format(j)} ")        
        f.write("\n")
        
    # skinned vertex data    
    f.write(f"{len(skinned_vertices)}\n")
    for vert in skinned_vertices:
        vert.print_me(f)
    # indices
    f.write(f"{len(indices)}\n")
    for idx in indices:
        f.write(f"{idx} ")
    f.write("\n")
    
    # animation count    
    f.write(f"{len(animations)}\n")
    for animation in animations:
        f.write(f"{animation.name}\n")
        f.write(f"{format(animation.duration)}\n")
        f.write(f"{len(animation.xform_cache)}\n")
        for k,v in animation.xform_cache.items():
            
            f.write(f"{k}\n")            
            f.write(f"{len(v)}\n")
            for s,_ in v:
                f.write(f"{format(s)} ")
            f.write("\n")
            for _,m in v:

                for i in m:
                    for j in i:                
                        f.write(f"{format(j)} ")


                f.write("\n")
    # animation_name
    # animation_duration
    # step count

    # node name
    # matrices

output_format()









# output_obj()