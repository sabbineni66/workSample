import numpy as np
import operator
import nltk
from collections import Counter
from nltk.corpus import wordnet as wn
from collections import Iterable
from collections import Counter
import numpy as np
import matplotlib.pyplot as plt
import sys
from webcolors import name_to_rgb
import math
from PIL import Image,ImageDraw

from PIL import Image, ImageDraw
import colorsys

def make_rainbow_rgb(colors, width, height):
    """colors is an array of RGB tuples, with values between 0 and 255"""

    img = Image.new("RGBA", (width, height))
    canvas = ImageDraw.Draw(img)

    def hsl(x):
        to_float = lambda x : x / 255.0
        (r, g, b) = map(to_float, x)
        h, s, l = colorsys.rgb_to_hsv(r,g,b)
        h = h if 0 < h else 1 # 0 -> 1
        return h, s, l

    rainbow = sorted(colors, key=hsl)

    dx = width / float(len(colors)) 
    x = 0
    y = height / 2.0
    for rgb in rainbow:
        canvas.line((x, y, x + dx, y), width=height, fill=rgb)
        x += dx
    #img.show() 
    return img

def makecounter(listallhypernym,input):
	for i in input:
		target=i[0]
		try:
			synset=wn.synsets(target)
		except UnicodeDecodeError:
			pass
		if synset==[] :
			pass
		else:	
			try:
				listallhypernym.append(hypernym(synset[0]))
			except IndexError:
				pass		
	count=Counter(flatten(listallhypernym))
	sumcounter=sum(count.values())
	return(count,sumcounter)

def hypernym(word):
	hypernym=word.hypernym_paths()
	return hypernym


def flatten(seq,container=None):
    if container is None:
        container = []
    for s in seq:
        if hasattr(s,'__iter__'):
            flatten(s,container)
        else:
            container.append(s)
    return container

def main():
	if len(sys.argv) == 2:
		space=sys.argv[1]
	if space=='fullcolors':
	 	f=open('fullcolorsdm.txt','r')
	 	colors=['green', 'blue', 'purple', 'pink', 'brown', 'red', 'yellow', 'orange', 'grey', 'teal', 'gray', 'violet', 'magenta', 'turquoise', 'rose', 'lavender', 'tan', 'olive', 'cyan', 'aqua', 'white', 'lime', 'beige', 'sienna', 'lilac', 'mauve', 'salmon', 'maroon', 'gold', 'peach', 'foam', 'drab', 'color', 'mustard', 'indigo', 'black', 'periwinkle', 'fuschia', 'sky', 'umber', 'plum', 'navy', 'mint']
	if space=='reducedcolors':
	 	f=open('reducedcolorsdm.txt','r')
	 	colors=['green', 'blue', 'purple', 'pink', 'brown', 'red', 'yellow', 'grey', 'teal', 'gray', 'violet', 'magenta', 'turquoise', 'cyan', 'white', 'beige', 'lilac', 'mauve', 'indigo', 'black']

	featuredict={}
	variancedict={}
	for line in f: 
		featurelist=[]
		line=line.split('\t')
		target=line[0]
		features=line[1].strip('\n')
		features=features.split(' ')
		for i in features: 
			featurelist.append(float(i))
		featuredict[target]=featurelist
		variance=np.var(featurelist)
		variancedict[target]=variance
	sortedvariance= sorted(variancedict.items(), key=operator.itemgetter(1))
	
	top500=sortedvariance[-500:]
	counttop,sumcountertop=makecounter([],top500)
	last500=sortedvariance[:500]
	countlast,sumcounterlast=makecounter([],last500)


	topvariance=counttop.most_common(50)
	leastvariance=countlast.most_common(50)

	topvariancecategories=[x[0].lemma_names()[0] for x in topvariance]
	leastvariancecategories=[x[0].lemma_names()[0] for x in leastvariance]

	categoriesonlytop= list(set(topvariancecategories).difference(leastvariancecategories))
	categoriesonlyleast=list(set(leastvariancecategories).difference(topvariancecategories))
	#print('categories with most variance:',categoriesonlytop)
	#print('categories with least variance:',categoriesonlyleast)

	# x_val= [x[0].lemma_names()[0] for x in leastvariance]
	# y_val = [x[1] for x in leastvariance]
	# print(x_val)
	# plt.bar(x_val,y_val,color='g')
	# plt.suptitle('least variance most common hypernyms', fontsize=12)
	# plt.xlabel('color')
	# plt.ylabel('frequency')
	# plt.xticks(rotation=20)
	# plt.rc('xtick', labelsize=5) 
	# plt.grid(True)
	# plt.show()

	



	#for each of these categories find the words that were in there + their features
	hypernymdict={}
	for x in categoriesonlytop:
		hypernymdict[x]=[]
		for i in top500:
			target=i[0]
			try:
				synset=wn.synsets(target)
			except UnicodeDecodeError:
				pass
			if synset==[]:
				pass
			else:	
				hypernymslist=hypernym(synset[0])
				hypernyms=[]
				for h in hypernymslist:
					for y in h:
						y=y.lemma_names()
						hypernyms.append(y)
						hypernyms=hypernyms=flatten(hypernyms)
				if x in hypernyms:
					hypernymdict[x].append(target)

					

					
	#find for each category 5 hypernyms with most variance 
	#distribution of colors per word
	for key,value in hypernymdict.iteritems():
		finaldictcolors={}
		category=key
		variancenew={}
		for i in value: 
			variancenew[i]=variancedict[i]
		sortedvariancenew= sorted(variancenew.items(), key=operator.itemgetter(1))
		hypernyms=sortedvariancenew[-5:]
		
		for x in hypernyms:
			colordict={}
			vector=featuredict[x[0]]
			summean=sum(vector)
			count=0
			for c in colors: 
				if vector[count]==0:
					colordict[c]=0
					count+=1
				else:
					colordict[c]=int((vector[count]/summean)*100)
					count+=1

			finaldictcolors[x]=colordict

	#distribution of colors per category

	# for key, value in hypernymdict.iteritems():
	# 	colordict={}
	# 	vectors=[]
	# 	for i in value:
	# 		vectors.append(featuredict[i])
	# 	mean=np.mean(np.array(vectors),axis=0)
	# 	summean=sum(mean)
	# 	count=0
	# 	for c in colors: 
	# 		if mean[count]==0:
	# 			colordict[c]=0
	# 			count+=1
	# 		else:
	# 			colordict[c]=int((mean[count]/summean)*100)
	# 			count+=1

	# 	finaldictcolors[key]=colordict


		for key,value in finaldictcolors.iteritems():
			pixels=[]
			name=key
			for key,value in finaldictcolors[key].iteritems():
				for i in range(value):
					if key =='color':
						pass
					elif key =='drab':
						pixels.append((50, 113, 23))
					elif key =='mint':
						pixels.append((152,255,152))
					elif key =='umber':
						pixels.append((	99, 81, 71))
					elif key =='sky':
						pixels.append((135,206,250))
					elif key =='rose':
						pixels.append(((255, 0, 128)))
					elif key =='fuschia':
						pixels.append((202, 44, 146))
					elif key =='peach':
						pixels.append((255,218,185))
					elif key=='mustard':
						pixels.append((255, 219, 88))
					elif key=='lilac':
						pixels.append((200,162,200))
					elif key =='mauve':
						pixels.append((224, 176, 255))
					elif key =='periwinkle':
						pixels.append((195,205,230))
					else:
						pixels.append(name_to_rgb(key))

			size=int(math.sqrt(len(pixels)))+1
			width=size*15
			image=make_rainbow_rgb(pixels, width, size)
			try:
				image.save('reducedcolorsviz/new/'+category+'/'+name[0]+'.png')
			except UnicodeDecodeError:
				pass



main()