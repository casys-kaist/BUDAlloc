import os, re, argparse
from parse_file import parse_file, DB
from classify import classifier, BINARY, USER, KERNEL, find_classifier_by_name, print_debug

TOTAL_TIME = 0
TOTAL_COUNT = 0

def func_classify(p_name, func_name, library) :
    if ("vmlinux" in library) or ("bpf_prog" in library):
        # Do kernel classification
        for classifier in KERNEL :
            result = classifier.classify(func_name)
            if result != None :
                return result
        
        return "kernel"

    elif ".so" in library :
        for classifier in USER :
            result = classifier.classify(func_name)
            if result != None :
                return result
        
        return "library"
    
    elif p_name in library :
        for classifier in BINARY :
            return classifier.classify(func_name)

    else :
        return "unknown"    

def classify(db) :
    print_debug(db.p_name)
    for record in db.data :
        cycle = record["cycles"]
        stack = record["stack"]
        stack_classified = []
        print_debug("cycle: %d"%cycle)

        # make classified stack
        for (func_name, library) in stack :
            classified = func_classify(db.p_name, func_name, library)
            stack_classified.append((func_name, classified))
            print_debug(func_name, library, classified)

        # Determine where to start
        classifier = None
        remained_stack = None
        start_func_name = None
        for (i, (func_name, classified)) in enumerate(stack_classified) :
            if classified == "unknown" :
                continue
            else :
                classifier = find_classifier_by_name(classified)
                remained_stack = stack_classified[i + 1:]
                start_func_name = func_name
                break
        
        if classifier == None :
            print_debug("Error: classifier is not found\n")
            continue

        # Determine where to insert
        classifier.add_by_rule(remained_stack, cycle, start_func_name)
    
if __name__ == "__main__" :
    parser = argparse.ArgumentParser()
    parser.add_argument("-f", "--file", dest="file", help = "File to parse")
    parser.add_argument("--simple", dest="simple", help = "simple print", action='store_true')
    args = parser.parse_args()

    db_pool, TOTAL_TIME, TOTAL_COUNT = parse_file(args.file)

    print("TOTAL ELAPSED TIME : %.5fs (100%%)\n\n" %(TOTAL_TIME))

    total = 0
    for db in db_pool.values() :
        print_brief = []
        # clean classifier
        for classifier in USER + KERNEL + BINARY :
            classifier.clean()

        classify(db)
        print("-------------------------------------------")
        print("[%s] Total time : %.5fs (%.3f%%)" %(
            db.p_name, 
            TOTAL_TIME / TOTAL_COUNT * db.total_count,
            float(db.total_count) / TOTAL_COUNT * 100))
        print("-------------------------------------------")

        for classifier in USER + KERNEL + BINARY :
            if not args.simple :
                print("%s: %.5fs (%.3f%%)"%(
                    classifier.name,
                    TOTAL_TIME / TOTAL_COUNT * classifier.count,
                    float(classifier.count) / TOTAL_COUNT * 100))
                for (func_name, cycle) in sorted(classifier.func_list.items(), key=lambda x: x[1], reverse=True) :
                    print("\t%-50s: %.5fs (%.3f%%)"%(
                        func_name,
                        TOTAL_TIME / TOTAL_COUNT * cycle,
                        float(cycle) / TOTAL_COUNT * 100))
                print("")
            else :
                print_brief.append((classifier.name, TOTAL_TIME / TOTAL_COUNT * classifier.count))
        
        if args.simple :
            for (name, _) in print_brief :
                if name not in ["library", "kernel"] :
                    print "%s\t"%name,
            print("")
            for (name, time) in print_brief :
                if name not in ["library", "kernel"] :
                    print "%.5f\t"%time,
            print("")
