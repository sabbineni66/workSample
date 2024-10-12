#Recursively changes '*.dmi' to 'icon/*/*.dmi'

import os, re, stat

def rework_dir(directory, repl_dict):
    print("Reworking: " + directory)
    for root, dirs, files in os.walk(directory):
        for file_name in files:
            try:
                path_to_new  = os.path.normpath(os.path.join(os.getcwd(), root))
                path_to_file = os.path.abspath(os.path.join(path_to_new, file_name))
                
                print(path_to_file)
                f = open(path_to_file, "r+")
                buf = f.read()

                for s in repl_dict:
                    if(s in buf):#rework
                        buf = re.sub(s, repl_dict[s], buf)
                
                #print(buf) #buff != null
                
                f.truncate()
                f.seek(0)
                f.write(buf)
                f.close()
                
            except Exception as e:
                print(e)
                print(file_name + " not accessable!")
        for d in dirs:
            rework_dir(root +"/"+ d, repl_dict)

def get_repl(directory):
    search_list = []
    need_list   = []
    repl_dict   = dict()

    for root, dirs, files in os.walk(directory):
        for file_name in files:
            #if(file_name.endswith('.dmi')):
            search_list.append('\'' + file_name + '\'')
            need_list.append(('\'' + os.path.normpath(root + "/" + file_name) + '\'').replace("\\", "/"))

        for d in dirs:
            repl_dict.update(get_repl(root + "/" + d))

        #creating dict
        for i in range(len(need_list)):
            repl_dict.update({search_list[i] : need_list[i]})

    #print(search_list, need_list)
    return repl_dict#search: need

def main(cdir, idir, mdir, sdir):
    repl_dict = get_repl(idir)
    repl_dict.update(get_repl(mdir))
    repl_dict.update(get_repl(sdir))
    print(repl_dict)
    rework_dir("maps", repl_dict)
    rework_dir(cdir, repl_dict)

icons_dir = "icons/"
code_dir  = "code/"
music_dir = "music/"
sound_dir = "sound/"
main(code_dir, icons_dir, music_dir, sound_dir)
