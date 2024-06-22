from func_name_extract import func_name_extract_from_code, func_name_extract_from_header
import re, os

USERCODE_PATH = os.path.dirname(os.path.realpath(__file__)) + "/../../../"
# KERNELCODE_PATH = "/home/babamba/sBPF-Kernel"
debug = False

def print_debug(*msg) :
    if debug :
        print(' '.join(map(str, msg)))

class classifier :
    def __init__(self, name, accpet_list, reject_list = [], rules = {}) :
        self.name = name
        self.accept_list = accpet_list
        self.reject_list = reject_list
        self.rule = self.make_rule(rules)
        self.count = 0
        self.func_list = {}

    def classify(self, func_name) :
        for accept in self.accept_list :
            if re.match(accept, func_name) :
                for reject in self.reject_list :
                    if re.match(reject, func_name) :
                        return None
                
                return self.name

        return None

    def make_rule(self, rules) :
        class_rule = {}
        class_rule[self.name] = "same"
        for (name, rule) in rules.items() :
            class_rule[name] = rule
        
        print_debug(class_rule)
        return class_rule
    
    def add_by_rule(self, stack, cycle, start_func_name) :
        classifier = self
        target_func = start_func_name
        
        for (func_name, classified) in stack :
            next_rule = classifier.rule.get(classified, None)
            if next_rule == None or next_rule == "same":
                continue
            elif next_rule == "change" :
                classifier = find_classifier_by_name(classified)
                target_func = func_name
            
        
        print_debug("class: " + classifier.name + "\n")
        num = classifier.func_list.get(target_func, 0)
        classifier.func_list[target_func] = num + cycle
        classifier.count += cycle

    def clean(self) :
        self.count = 0
        self.func_list = {}
        

def find_classifier_by_name(name) :
    if name == "binary" :
        return BINARY[0]
    
    for classifier in USER :
        if classifier.name == name :
            return classifier
    
    for classifier in KERNEL :
        if classifier.name == name :
            return classifier
    
    return None

# User part classification
USER = []
USER.append(classifier(
                "alias_alloc",
                func_name_extract_from_code(USERCODE_PATH + "/libkernel/src/alloc/alloc_alias.c"),
                [".*ota_free.*", "try_deferred_free"],
                rules = {"canonical_alloc": "change", "canonical_free": "change", "kernel": "change"}
))
USER.append(classifier(
                "canonical_alloc",
                func_name_extract_from_code(USERCODE_PATH + "/libkernel/src/alloc/alloc_canonical.c") + ["mi_.*", "_mi_.*"],
                [".*canonical_.*free.*", ".*mi_free.*", ".*mi_page_free.*"],
                rules = {"kernel": "change"}
))
USER.append(classifier(
                "alias_free",
                [".*ota_free.*", "try_deferred_free"],
                rules = {"alias_alloc": "change", "canonical_free": "change", "kernel": "change"}
))
USER.append(classifier(
                "canonical_free",
                [".*canonical_.*free.*", ".*mi_free.*", ".*mi_page_free.*"],
                rules = {"kernel" : "change"}
))
USER.append(classifier(
                "library",
                [".*"],
                rules = {"alias_alloc": "change", "canonical_alloc": "change", 
                "alias_free": "change", "canonical_free": "change", "kernel": "change"}
))

# Binary classification
BINARY = []
BINARY.append(classifier(
                "binary",
                [".*"],
                rules = {"alias_alloc": "change", "canonical_alloc": "change", "kernel": "change", 
                "alias_free": "change", "canonical_free": "change", "library" : "change"}
))

# Kernel part classification
KERNEL = []
KERNEL.append(classifier(
                "uaddr_to_kaddr",
                [".*uaddr_to_kaddr"],
                rules = {"reverse_map": "change", "pte": "change", "tlb_flush": "change", "rcu_lock": "change", "sbpf_page_fault": "change", "sbpf_call_function": "change"}
))
KERNEL.append(classifier(
                "reverse_map",
                ["sbpf_reverse.*"],
                rules = {"uaddr_to_kaddr": "change", "pte": "change", "tlb_flush": "change", "rcu_lock": "change", "sbpf_page_fault": "change", "sbpf_call_function": "change"}
))
KERNEL.append(classifier(
                "sbpf_page_fault",
                ["sbpf_handle_page_fault.*"],
                rules = {"sbpf_call_function": "change", "uaddr_to_kaddr": "change", "reverse_map": "change", "pte": "change", "tlb_flush": "change", "rcu_lock": "change"}
))
KERNEL.append(classifier(
                "sbpf_call_function",
                ["sbpf_call_function.*"],
                rules = {"sbpf_page_fault": "change", "uaddr_to_kaddr": "change", "reverse_map": "change", "pte": "change", "tlb_flush": "change", "rcu_lock": "change"}
))
KERNEL.append(classifier(
                "pte",
                [".*pte.*"],
                rules = {"uaddr_to_kaddr": "change", "reverse_map": "change", "rcu_lock": "change", "tlb_flush": "change", "sbpf_page_fault": "change", "sbpf_call_function": "change"}
))
KERNEL.append(classifier(
                "tlb_flush",
                [".*tlb_finish.*", ".*tlb_flush.*", ".*flush_tlb.*", "tlb_table_flush"],
                rules = {"uaddr_to_kaddr": "change", "reverse_map": "change", "pte": "change", "rcu_lock": "change", "sbpf_page_fault": "change", "sbpf_call_function": "change"}
))
KERNEL.append(classifier(
                "rcu_lock",
                [".*rcu_read_.*"],
                rules = {"uaddr_to_kaddr": "change", "reverse_map": "change", "pte": "change", "tlb_flush": "change", "sbpf_page_fault": "change", "sbpf_call_function": "change"}
))
KERNEL.append(classifier(
                "kernel",
                [".*"],
                rules = {"uaddr_to_kaddr": "change", "reverse_map": "change", "pte": "change", "tlb_flush": "change", "rcu_lock": "change", "sbpf_page_fault": "change", "sbpf_call_function": "change"}
))