

# nodes
#   name of node
#   children

class Node:
    def __init__(self, name, index):
        ...
        self.name = name
        self.index = index
        self.children = []

nodes = {}
indices = []
skinned_vertices = {}
class SkinnedVertex:
    def __init__(self, position, normal, uv, indices, weights):
        self.position   = position
        self.normal     = normal
        self.uv         = uv
        self.indices    = indices
        self.weights    = weights


        
import bpy

D = bpy.data

bpy.context.view_layer.update()

arm = D.armatures.items()

print("---")
arm = arm[0][1]
for i, bone in enumerate(arm.bones):
    if bone.parent:
        nodes[bone.parent.name].children.append(bone.name)
    nodes[bone.name] = Node(bone.name, i)
    print(f"{i}: {bone.name}")
    print(bone.matrix_local.inverted_safe())
    print(bone.matrix_local)

for node in nodes.values():
#    print(f"{node.name}: {[i for i in node.children]}")
    ...   



mutant_mesh = D.meshes[D.meshes.find("MutantMesh")]
vertices = mutant_mesh.vertices
loops = mutant_mesh.loops
uv = mutant_mesh.uv_layers


# poly.index is the number of triangles, so x3 is indices
# loops is indices of mesh loops that make up the triangle

verts = {}
norms = {}
uvs = {}

vg = D.objects[1].vertex_groups
vertex_groups = {}
for v in vg:
    vertex_groups[v.index] = v.name

for poly in mutant_mesh.loop_triangles:
    
    for i in range(3):
        indices.append(poly.vertices[i])
        verts[poly.vertices[i]]             = vertices[poly.vertices[i]].co
        norms[poly.vertices[i]]             = vertices[poly.vertices[i]].normal
        uvs[poly.vertices[i]]               = uv[2].uv[poly.vertices[i]].vector
        ind = []
        w = []
        for g in vertices[poly.vertices[i]].groups:
            ind.append(nodes[vertex_groups[g.group]].index)
            w.append(g.weight)
        skinned_vertices[poly.vertices[i]]  = SkinnedVertex(vertices[poly.vertices[i]].co,
                                                vertices[poly.vertices[i]].normal,
                                                uv[2].uv[poly.vertices[i]].vector,
                                                ind,
                                                w
                                                )
    

#for vert in skinned_vertices.values():
#    print("---")
#    print([f"{v:12.6f}" for v in vert.position])
#    print([f"{v:12.6f}" for v in vert.uv])
#    print([f"{v:12.6f}" for v in vert.normal])
#    print(vert.indices)
#    print([f"{v:12.6f}" for v in vert.weights])

# grab the animations            
# grab inverse bind matrices



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
    print(max(vertices.keys()))
    for i in range(0, len(indices), 3):
        f.write("f ")
        for j in range(3):
            f.write(f"{vertices[indices[i + j]] + 1}")
            f.write(f"/{normals[indices[i + j]] + 1}")
            f.write(f"/{uvs2[indices[i + j]] + 1} ")
        f.write("\n")


action = D.actions[D.actions.find("Mutant.Punch.100_Armature")]
arm2 = D.objects[0].animation_data
for group in action.groups:
    #print(group.name, len(group.channels))
    for curve in group.channels:
        #print(curve.data_path)
        ...

# for each time you want to sample
# iterate over each bone and figure out its rotation, scale and translation

for node in bpy.context.scene.objects:
    if node.type == "ARMATURE":
        for bone in node.pose.bones:
            print(bone.name, bone.rotation_quaternion, bone.scale, bone.matrix)
        
        ...



# output_obj()
