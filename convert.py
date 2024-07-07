import json

def convert_abilities():
    lines  = open("./data/formats/abilities.txt").read().split("\n")[:-1]
    count = int(lines[0])
    idx = 1
    abilities = {}
    for i in range(count):
        name = lines[idx]
        idx += 1

        function_ptr = lines[idx]
        idx += 1

        cooldown = lines[idx]
        idx += 1
        m = {
            "use_ptr_idx": function_ptr,
            "cooldown": cooldown
                }
        abilities[name] = m
    __import__('pprint').pprint(abilities)
    f = open("./data/formats/abilities.json", "w")
    f.write(json.dumps(abilities))

def convert_buffers():
    lines  = open("./data/formats/buffers.txt").read().split("\n")[:-1]
    count = int(lines[0])
    idx = 1
    buffers = {}
    for _ in range(count):
        name = lines[idx]
        idx += 1

        attribute_count = int(lines[idx])
        idx += 1
        attributes = []
        for _ in range(attribute_count):
            m = {}
            a = [int(i) for i in lines[idx].strip().split(" ")]
            m["count"]  = a[0]
            m["type"]   = a[1]
            attributes.append(m)
            idx += 1

        m = {
            "attributes": attributes,
            }
        buffers[name] = m
    __import__('pprint').pprint(buffers)
    f = open("./data/formats/buffers.json", "w")
    f.write(json.dumps(buffers))

def convert_enemy():
    lines  = open("./data/formats/enemy.txt").read().split("\n")[:-1]
    count = int(lines[0])
    idx = 1
    enemies = {}
    for _ in range(count):
        enemy_type = int(lines[idx])
        idx += 1

        hp = int(lines[idx])
        idx += 1

        radius = float(lines[idx])
        idx += 1
        name = lines[idx]
        idx += 1

        m = {
            "type": enemy_type,
            "hp": hp,
            "radius": radius,
        }
        enemies[name] = m
    __import__('pprint').pprint(enemies)
    f = open("./data/formats/enemy.json", "w")
    f.write(json.dumps(enemies))

def convert_hero():
    lines  = open("./data/formats/hero.txt").read().split("\n")[:-1]
    count = int(lines[0])
    idx = 1
    heroes = {}
    for _ in range(count):
        name = lines[idx]
        idx += 1

        ability_count = int(lines[idx])
        idx += 1
        abilities = []

        for _ in range(ability_count):
            abilities.append(lines[idx])
            idx+=1

        m = {
            "abilities": abilities,
        }
        heroes[name] = m
    __import__('pprint').pprint(heroes)
    f = open("./data/formats/hero.json", "w")
    f.write(json.dumps(heroes))

def convert_models():
    lines  = open("./data/formats/models.txt").read().split("\n")[:-1]
    count = int(lines[0])
    idx = 1
    models = {}
    for _ in range(count):
        name = lines[idx]
        idx += 1

        m = {
            "location": lines[idx],
        }
        idx += 1
        models[name] = m
    __import__('pprint').pprint(models)
    f = open("./data/formats/models.json", "w")
    f.write(json.dumps(models))

def convert_render_data():
    lines  = open("./data/formats/render_data.txt").read().split("\n")[:-1]
    count = int(lines[0])
    idx = 1
    render_data = {}
    for _ in range(count):
        m = {
        }
        name = lines[idx]
        idx += 1
        animation = lines[idx]
        idx += 1
        if animation != '0':
            assert False and "Shouldn't have animation?"

        m["texture"] = lines[idx]
        idx += 1
        m["shader"] = lines[idx]
        idx += 1
        m["scale"] = float(lines[idx])
        idx += 1
        m["buffer"] = lines[idx]
        idx += 1

        render_data[name] = m
    __import__('pprint').pprint(render_data)
    f = open("./data/formats/render_data.json", "w")
    f.write(json.dumps(render_data))


def convert_shaders():
    lines  = open("./data/formats/shader.txt").read().split("\n")[:-1]
    count = int(lines[0])
    idx = 1
    shader_dict = {}
    for _ in range(count):
        name = lines[idx]
        idx += 1
        shader_count = int(lines[idx])
        idx += 1
        shader = []
        for _ in range(shader_count):
            ma = {}
            a = lines[idx].strip().split(" ")
            ma["type"] = int(a[0])
            ma["location"] = a[1]
            shader.append(ma)
            idx += 1

        shader_dict[name] = shader
    __import__('pprint').pprint(shader_dict)
    f = open("./data/formats/shader.json", "w")
    f.write(json.dumps(shader_dict))


def convert_textures():
    lines  = open("./data/formats/textures.txt").read().split("\n")[:-1]
    count = int(lines[0])
    idx = 1
    textures = {}
    for _ in range(count):
        name = lines[idx]
        idx += 1
        location = lines[idx]
        idx += 1


        textures[name] = {"location": location}
    __import__('pprint').pprint(textures)
    f = open("./data/formats/textures.json", "w")
    f.write(json.dumps(textures))

def convert_map01():
    lines  = open("./data/maps/map01.txt").read().split("\n")[:-1]
    map = {}
    map["model"] =  lines[0]
    map["scale"] =  float(lines[1])
    map["static_geometry"] = lines[2]

    __import__('pprint').pprint(map)
    f = open("./data/maps/map01.json", "w")
    f.write(json.dumps(map))
def convert_map01_static():
    lines  = open("./data/maps/map01_static.txt").read().split("\n")[:-1]
    count = int(lines[0])
    idx = 1
    static_geometry = {}
    for _ in range(count):
        name = lines[idx]
        idx += 1
        model = lines[idx]
        idx += 1
        x = float(lines[idx])
        idx += 1
        y = float(lines[idx])
        idx += 1
        z = float(lines[idx])
        idx += 1


        static_geometry[name] = {
                "model":model,
                "position":[x,y,z]
                }
    __import__('pprint').pprint(static_geometry)
    f = open("./data/maps/map01_static.json", "w")
    f.write(json.dumps(static_geometry))
    
def main()   ->int:
    

    return 0


if __name__ == "__main__":
    pass
exit(main())
