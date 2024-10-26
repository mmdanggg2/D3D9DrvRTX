
#ifndef _C_RBTREE_
#define _C_RBTREE_

template <class ClassT> class rbtree_allocator {
public:
	typedef typename ClassT::node_t node_t;

public:
	rbtree_allocator() {
	}
	~rbtree_allocator() {
	}

	node_t *alloc_node(void) {
		return new node_t;
	}

	void free_node(node_t *pNode) {
		delete pNode;
	}
};

template <class KeyT, class DataT> class rbtree {
private:
	enum { RED_NODE, BLACK_NODE };
	enum { Left = 0, Right = 1 };

public:
	struct node_t {
		struct node_t *pC[2], *pParent;
		unsigned char color;
		KeyT key;
		DataT data;
	};

private:
	rbtree(const rbtree &);
	rbtree &operator=(const rbtree &);

public:
	rbtree() {
		m_pRoot = &m_NIL;
		m_NIL.color = BLACK_NODE;
		m_NIL.pParent = 0;
		m_NIL.pC[Left] = 0;
		m_NIL.pC[Right] = 0;
	}
	~rbtree() {
	}

	node_t * FASTCALL find(KeyT key) {
		node_t *px;

		px = m_pRoot;
		while (px != &m_NIL) {
			if (key == px->key) {
				return px;
			}
			px = px->pC[(key < px->key) ? Left : Right];
		}

		return 0;
	}

	bool FASTCALL insert(node_t *pNode) {
		//Attempt to insert the new node
		if (m_pRoot == &m_NIL) {
			m_pRoot = pNode;
			pNode->pParent = &m_NIL;
			pNode->pC[Left] = &m_NIL;
			pNode->pC[Right] = &m_NIL;
			pNode->color = BLACK_NODE;

			return true;
		}

		node_t *px, *py;
		unsigned int compLR = Left;

		px = m_pRoot;
		py = 0;

		while (px != &m_NIL) {
			py = px;

			if (pNode->key == px->key) {
				return false;
			}

			compLR = (pNode->key < px->key) ? Left : Right;
			px = px->pC[compLR];
		}

		py->pC[compLR] = pNode;
		pNode->pParent = py;
		pNode->pC[Left] = &m_NIL;
		pNode->pC[Right] = &m_NIL;
		pNode->color = RED_NODE;

		//Rebalance the tree
		px = pNode;
		node_t *pxParent;
		while (((pxParent = px->pParent) != &m_NIL) && (pxParent->color == RED_NODE)) {
			node_t *pxParentParent;

			pxParentParent = pxParent->pParent;
			pxParentParent->color = RED_NODE;
			if (pxParent == pxParentParent->pC[Left]) {
				py = pxParentParent->pC[Right];
				if (py->color == RED_NODE) {
					py->color = BLACK_NODE;
					pxParent->color = BLACK_NODE;
					px = pxParentParent;
				}
				else {
					if (px == pxParent->pC[Right]) {
						px = pxParent;
						left_rotate(px);
						pxParent = px->pParent;
					}
					pxParent->color = BLACK_NODE;
					right_rotate(pxParentParent);
				}
			}
			else {
				py = pxParentParent->pC[Left];
				if (py->color == RED_NODE) {
					py->color = BLACK_NODE;
					pxParent->color = BLACK_NODE;
					px = pxParentParent;
				}
				else {
					if (px == pxParent->pC[Left]) {
						px = pxParent;
						right_rotate(px);
						pxParent = px->pParent;
					}
					pxParent->color = BLACK_NODE;
					left_rotate(pxParentParent);
				}
			}
		}

		m_pRoot->color = BLACK_NODE;

		return true;
	}

	void FASTCALL remove(node_t *pNode) {
		node_t *px, *py;

		py = pNode;
		if (py->pC[Left] == &m_NIL) {
			px = py->pC[Right];
		}
		else if (py->pC[Right] == &m_NIL) {
			px = py->pC[Left];
		}
		else {
			py = py->pC[Right];
			while (py->pC[Left] != &m_NIL) {
				py = py->pC[Left];
			}
			px = py->pC[Right];
		}

		if (py != pNode) {
			pNode->pC[Left]->pParent = py;
			py->pC[Left] = pNode->pC[Left];
			if (py != pNode->pC[Right]) {
				px->pParent = py->pParent;
				py->pParent->pC[Left] = px;
				py->pC[Right] = pNode->pC[Right];
				pNode->pC[Right]->pParent = py;
			}
			else {
				px->pParent = py;
			}

			if (m_pRoot == pNode) {
				m_pRoot = py;
			}
			else {
				pNode->pParent->pC[(pNode->pParent->pC[Left] == pNode) ? Left : Right] = py;
			}
			py->pParent = pNode->pParent;

			unsigned char tempColor = pNode->color;
			pNode->color = py->color;
			py->color = tempColor;

			py = pNode;
		}
		else {
			px->pParent = py->pParent;

			if (m_pRoot == pNode) {
				m_pRoot = px;
			}
			else {
				py->pParent->pC[(py->pParent->pC[Left] == py) ? Left : Right] = px;
			}
		}

		if (py->color == BLACK_NODE) {
			node_t *pxp;

			while (((pxp = px->pParent) != &m_NIL) && (px->color == BLACK_NODE)) {
				if (px == pxp->pC[Left]) {
					node_t *pw;

					pw = pxp->pC[Right];
					if (pw->color == RED_NODE) {
						pw->color = BLACK_NODE;
						pxp->color = RED_NODE;
						left_rotate(pxp);
						pw = pxp->pC[Right];
					}
					//if ((pw->pC[Left]->color == BLACK_NODE) & (pw->pC[Right]->color == BLACK_NODE))
					if (((pw->pC[Left]->color ^ BLACK_NODE) | (pw->pC[Right]->color ^ BLACK_NODE)) == 0) {
						pw->color = RED_NODE;
						px = pxp;
					}
					else {
						if (pw->pC[Right]->color == BLACK_NODE) {
							pw->pC[Left]->color = BLACK_NODE;
							pw->color = RED_NODE;
							right_rotate(pw);
							pw = pxp->pC[Right];
						}
						pw->color = pxp->color;
						pxp->color = BLACK_NODE;
						pw->pC[Right]->color = BLACK_NODE;
						left_rotate(pxp);
						break;
					}
				}
				else {
					node_t *pw;

					pw = pxp->pC[Left];
					if (pw->color == RED_NODE) {
						pw->color = BLACK_NODE;
						pxp->color = RED_NODE;
						right_rotate(pxp);
						pw = pxp->pC[Left];
					}
					//if ((pw->pC[Right]->color == BLACK_NODE) & (pw->pC[Left]->color == BLACK_NODE))
					if (((pw->pC[Right]->color ^ BLACK_NODE) | (pw->pC[Left]->color ^ BLACK_NODE)) == 0) {
						pw->color = RED_NODE;
						px = pxp;
					}
					else {
						if (pw->pC[Left]->color == BLACK_NODE) {
							pw->pC[Right]->color = BLACK_NODE;
							pw->color = RED_NODE;
							left_rotate(pw);
							pw = pxp->pC[Left];
						}
						pw->color = pxp->color;
						pxp->color = BLACK_NODE;
						pw->pC[Left]->color = BLACK_NODE;
						right_rotate(pxp);
						break;
					}
				}
			}
			px->color = BLACK_NODE;
		}

		return;
	}

	node_t * FASTCALL next_node(node_t *pNode) {
		node_t *px;

		if (pNode->pC[Right] != &m_NIL) {
			pNode = pNode->pC[Right];
			while (pNode->pC[Left] != &m_NIL) {
				pNode = pNode->pC[Left];
			}
			return pNode;
		}

		if (pNode->pParent == &m_NIL) {
			return &m_NIL;
		}

		px = pNode->pParent;
		while (pNode == px->pC[Right]) {
			pNode = px;
			if (pNode->pParent == &m_NIL) {
				return &m_NIL;
			}
			px = pNode->pParent;
		}

		return px;
	}

	node_t *begin(void) {
		node_t *px;

		if (m_pRoot == &m_NIL) {
			return &m_NIL;
		}

		for (px = m_pRoot; px->pC[Left] != &m_NIL; px = px->pC[Left]);

		return px;
	}

	node_t *end(void) {
		return &m_NIL;
	}

	void FASTCALL clear(rbtree_allocator<rbtree<KeyT, DataT> > *pAllocator) {
		node_t *px;

		px = begin();
		while (px != end()) {
			node_t *py = next_node(px);
			remove(px);
			pAllocator->free_node(px);
			px = py;
		}

		return;
	}

	unsigned int calc_size(void) {
		node_t *px;
		unsigned int size;

		px = begin();
		size = 0;
		while (px != end()) {
			px = next_node(px);
			size++;
		}

		return size;
	}

private:
	void FASTCALL left_rotate(node_t *px) {
		node_t *py;

		py = px->pC[Right];

		node_t *pyLeft = py->pC[Left];
		px->pC[Right] = pyLeft;
		if (pyLeft != &m_NIL) {
			pyLeft->pParent = px;
		}

		node_t *pxParent = px->pParent;
		py->pParent = pxParent;

		if (pxParent == &m_NIL) {
			m_pRoot = py;
		}
		else {
			pxParent->pC[(px == pxParent->pC[Left]) ? Left : Right] = py;
		}

		py->pC[Left] = px;
		px->pParent = py;

		return;
	}

	void FASTCALL right_rotate(node_t *px) {
		node_t *py;

		py = px->pC[Left];

		node_t *pyRight = py->pC[Right];
		px->pC[Left] = pyRight;
		if (pyRight != &m_NIL) {
			pyRight->pParent = px;
		}

		node_t *pxParent = px->pParent;
		py->pParent = pxParent;

		if (pxParent == &m_NIL) {
			m_pRoot = py;
		}
		else {
			pxParent->pC[(px == pxParent->pC[Right]) ? Right : Left] = py;
		}

		py->pC[Right] = px;
		px->pParent = py;

		return;
	}

private:
	node_t m_NIL;
	node_t *m_pRoot;
};

template <class ClassT> class rbtree_node_pool {
public:
	typedef typename ClassT::node_t node_t;

public:
	rbtree_node_pool() {
		m_pTail = 0;
	}
	~rbtree_node_pool() {
	}

	inline void FASTCALL add(node_t* pNode) {
		pNode->pParent = m_pTail;

		m_pTail = pNode;

		return;
	}

	inline node_t* try_remove(void) {
		node_t* pNode;

		if (m_pTail == 0) {
			return 0;
		}

		pNode = m_pTail;

		m_pTail = pNode->pParent;

		return pNode;
	}

	unsigned int calc_size(void) {
		node_t* pNode;
		unsigned int size;

		pNode = m_pTail;
		size = 0;
		while (pNode != 0) {
			pNode = pNode->pParent;
			size++;
		}

		return size;
	}

private:
	node_t* m_pTail;
};

#endif //_C_RBTREE_
