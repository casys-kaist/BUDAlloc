import os, sys

class DB :
    def __init__(self, p_name) :
        self.p_name = p_name
        self.data = []
        self.total_count = 0

    # def check_db(self) :
    #     total_count = self.data["total_count"]
    #     count = self.unknown
        
    #     for key, dic in self.data.items() :
    #         if key == "total_count" :
    #             continue

    #         for value in dic.values() :
    #             count += value[0]
        
    #     return (count == total_count, count, total_count)

DB_pool = {}
START_TIME = 0
END_TIME = 0
TOTAL_COUNT = 0

def load_file(file_name) :
    if os.path.isabs(file_name) :
        file_path = file_name
    else :
        file_path = os.path.join(os.path.dirname(__file__), file_name)
        
    file = open(file_path, 'r')
    if not file :
        print("Error: file not found")
        return None
    
    return file

def parse_line(db, line) :
    line_split = line.lstrip("\t").lstrip(" ").rstrip().split(" (")
    func_name = " ".join(line_split[0].split(" ")[1:]).split("+")[0]
    library = line_split[1].rstrip(")")

    return (func_name, library)

def edit_db(file, line, start) :
    global START_TIME, END_TIME, TOTAL_COUNT
    prog_info = filter(None, line.rstrip().split(" "))
    prog_name = prog_info[0]
    # pid = prog_info[1]
    cycles = int(prog_info[-2])
    time = float(prog_info[-3].rstrip(":"))
    
    if prog_name not in DB_pool.keys() :
        DB_pool[prog_name] = DB(prog_name)
    
    db = DB_pool[prog_name]

    if start :
        START_TIME = time
    else :
        END_TIME = time
    
    sequence = {"stack": [], "cycles": cycles}
    while True :
        line = file.readline()
        if not line or line == "\n" :
            break

        sequence["stack"].append(parse_line(db, line))
    
    sequence["stack"].reverse()
    db.data.append(sequence)
    db.total_count += cycles
    TOTAL_COUNT += cycles

def parse_file(file_name) :
    file = load_file(file_name)
    if not file :
        return None

    # First line must be <program name> <pid> ...
    line = file.readline()
    edit_db(file, line, True)

    is_record = True
    while True :
        line = file.readline()
        if not line : # EOF
            break

        elif line == "\n" :
        # next line will be record or EOF
            is_record = True
            continue

        elif is_record :
            edit_db(file, line, False)
            is_record = True
            continue

    return DB_pool, END_TIME - START_TIME, TOTAL_COUNT

if __name__ == "__main__" :
    db, total_time = parse_file("../out.perf")
    print("total_time: {}".format(total_time))