import numpy as np
import utils
import sys
import itertools
import composes
from composes.utils import io_utils
from composes.similarity.cos import CosSimilarity 
from collections import defaultdict
import matplotlib.pyplot as plt
import pickle

def functionneighbours(words,number): 
    #load a space
    if sys.argv[2]=='full':
        my_space = io_utils.load("./data/out/thesisfull.pkl")
    if sys.argv[2]=='nonzero':
        my_space = io_utils.load("./data/out/thesis.pkl")

    return(my_space.get_neighbours(words,number, CosSimilarity()))

def main():
    if len(sys.argv) == 3:
       # listtop500 = open('data/listtop500.txt','r')
        #listlast500=open('data/listlast500.txt','r')


        #['reykjanes','danielli','underdrawing','halichoerus','hepler','change-']


        #['widgeon','colourpoint','water-lilies','kingbirds','gallinules','pebbledash']

        #['flowers.','nard','hearing-aid','filsham','trumpet-shaped','crecca' ]

       #['kerchief', 'kingbirds','cerise','biretta','pale-blue','v-necked','pebbledash']
         
        #['crecca','flowers.','corollas','shovelers','supercilium','crocuses']
        listtop500=['village','ponk','catspaw','lycaenid','orangey-pink','saponaria']
        if sys.argv[2]=='full':
            semanticspace1=utils.readDM("data/colorswithoutremovedtargets.dm")
        if sys.argv[2]=='nonzero':
            semanticspace1=utils.readDM("data/reducedcolors.dm")
        dicttop=defaultdict(list)
        dictlast=defaultdict(list)
        for line in listtop500:
            #word = line.strip()
            word=line
            num_neighbours = int(sys.argv[1])
            neighbours1=[]
            for i in utils.neighbours(semanticspace1,semanticspace1[word],num_neighbours):
                neighbours1.append(i.strip("."))
            neighbours2=[]
            cosinefull=[]
            for i in functionneighbours(word,num_neighbours):
                neighbours2.append(i[0])
                cosinefull.append(i[1])
            densityfull=(sum(cosinefull))/(len(cosinefull))


            ##compare neigbours of 2 different spaces 
            intersection = set(neighbours1) & set(neighbours2)

       
            print(word, intersection,len(intersection))
            
            #dicttop[word]=len(intersection)
           

        # for line in listlast500:
        #     word = line.strip()
        #     num_neighbours = int(sys.argv[1])
        #     neighbours1=[]
        #     for i in utils.neighbours(semanticspace1,semanticspace1[word],num_neighbours):
        #         neighbours1.append(i.strip("."))
        #     neighbours2=[]
        #     cosinefull=[]
        #     for i in functionneighbours(word,num_neighbours):
        #         neighbours2.append(i[0])
        #         cosinefull.append(i[1])
        #     densityfull=(sum(cosinefull))/(len(cosinefull))


        #     ##compare neigbours of 2 different spaces 
        #     intersection = set(neighbours1) & set(neighbours2)
        #     print(intersection,len(intersection))
            
        #     dictlast[word]=len(intersection)
           


        #density color space 
            listdensity=[]
            listcoherence=[]
            neighbours=utils.neighbours(semanticspace1,semanticspace1[word],num_neighbours)
            for i in neighbours:
                cosine=utils.cosine_similarity(semanticspace1[word],semanticspace1[i])
               
                # if np.isnan(cosine):
                #     pass
                # else: 
                listdensity.append(cosine)
            density=sum(listdensity)/(len(listdensity))
            print('density color space: ',density)
            print('density full space: ', densityfull)
            dicttop[word]=[len(intersection),density]
       


        #     #calculate coherence color space ( = average of cosine of every point in the nearest neighbours to every other point)
        #     #coherence color space 

        #     combinations= list(itertools.combinations(neighbours,2))
        #     # #print(combinations)
        #     for x in combinations:
        #         cosine=utils.cosine_similarity(semanticspace1[x[0]],semannticspace1[x[1]])
        #         if np.isnan(cosine):
        #             pass
        #         else:
        #             listcoherence.append(cosine)
        #     coherence=sum(listcoherence)/len(listcoherence)
        #     print('coherence: ',coherence)

        #     dicttop[word]=[len(intersection),density,coherence]
        

         #create graph 
        #pickle.dump( dicttop, open( "dicttop"+sys.argv[2]+".p", "wb" ) )
        #pickle.dump( dictlast, open( "dictlast"+sys.argv[2]+".p", "wb" ) )

        #plt.plot(range(len(dicttop)), dicttop.values() ,'ro' )
        #plt.plot(range(len(dictlast)),dictlast.values(),'bo')
        #plt.show()


           

        

main()
