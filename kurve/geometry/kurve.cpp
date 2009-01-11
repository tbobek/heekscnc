// written by g.j.hawkesford 2006 for Camtek Gmbh

#include "geometry.h"

#ifdef PEPSPOST
	#include "postoutput.h"
#endif


////////////////////////////////////////////////////////////////////////////////////////////////
// kurve
////////////////////////////////////////////////////////////////////////////////////////////////

namespace geoff_geometry {

	SpanVertex::SpanVertex() {
		for(int i = 0; i < SPANSTORAGE; i++) index[i] = NULL;
	}

	SpanVertex::~SpanVertex() {
#ifndef PEPSDLL 
		// don't know what peps did about this?
		for(int i = 0; i < SPANSTORAGE; i++) {
			if(index[i] != NULL) {
				delete index[i];
			}
		}
#endif
	}

	const SpanVertex& SpanVertex::operator= (const SpanVertex& spv ){
		/// 

		memcpy(x, spv.x, SPANSTORAGE * sizeof(double));
		memcpy(y, spv.y, SPANSTORAGE * sizeof(double));
		memcpy(xc, spv.xc, SPANSTORAGE * sizeof(double));
		memcpy(yc, spv.yc, SPANSTORAGE * sizeof(double));

		for(unsigned int i = 0; i < SPANSTORAGE; i++) {
			type[i] = spv.type[i];
			spanid[i] = spv.spanid[i];
			index[i] = spv.index[i];
#ifndef PEPSDLL
			if(index[i] != NULL) {
				SpanDataObject* obj = new SpanDataObject(index[i]);
				index[i] = obj;
			}
#endif
		}
		return *this;
	}


	void SpanVertex::Add(int offset, int spantype, const Point& p, const Point& pc, int ID)
	{
		type[offset] = spantype;
//		index[offset] = NULL;
		x[offset] = p.x;
		y[offset] = p.y;
		xc[offset] = pc.x;
		yc[offset] = pc.y;
		spanid[offset] = ID;
	}
	void SpanVertex::AddSpanID(int offset, int ID)
	{
		spanid[offset] = ID;
	}

#if PEPSDLL
	void SpanVertex::Add(int offset, WireExtraData* Index )
	{
		index[offset] = Index;
	}
	WireExtraData* SpanVertex::Get(int offset)
	{
		return index[offset];
	}
#else
	void SpanVertex::Add(int offset, const SpanDataObject* Index ) {
		index[offset] = Index;
	}
		
	const SpanDataObject* SpanVertex::GetIndex(int offset) const{
		return index[offset];
	}
#endif

	int SpanVertex::Get(int offset, Point& pe, Point& pc)
	{
		pe = Point(x[offset], y[offset]);
		pc = Point(xc[offset], yc[offset]);

		return type[offset];
	}
	int SpanVertex::GetSpanID(int offset)
	{
		return spanid[offset];
	}

	Span Span::Offset(double offset)
	{
		Span Offsp = *this;
		if(FNEZ(offset) && !NullSpan) {
			if ( !dir ) {
				// straight
				Offsp.p0.x -= offset * vs.gety();
				Offsp.p0.y += offset * vs.getx();

				Offsp.p1.x -= offset * vs.gety();
				Offsp.p1.y += offset * vs.getx();
			}
			else {
				//	 circular span
				//	double coffset = (double) dir * offset;
				Offsp.p0.x -= vs.gety() * offset;
				Offsp.p0.y += vs.getx() * offset;

				Offsp.p1.x -= ve.gety() * offset;
				Offsp.p1.y += ve.getx() * offset;

//				Offsp.radius -= dir * offset;
			}
			Offsp.SetProperties(true);
		}
		return Offsp;
	}

	bool Span::JoinSeparateSpans(Span& sp) {
		// this method joins this span to sp where they are separated normally by an offset from original
		//
		// parameters:-
		//	Input sp	near span
		//	Output	this->p1 and sp.p0 assigned to the spans intersection
		Point inters;
		int turnLeft = ((this->ve ^ sp.vs) > 0)? 1 : -1;
		if(!this->dir) {
			CLine one(*this);
			if(!sp.dir) {
				// line line
				CLine two(sp);
				inters = one.Intof(two);
			}
			else {
				// line arc
				Circle two(sp);
				inters = one.Intof(-turnLeft * sp.dir, two);
			}
		}
		else {
			Circle one(*this);
			if(!sp.dir) {
				// arc line
				CLine two(sp);
				inters = two.Intof(turnLeft * this->dir, one);
			}
			else {
				// arc arc
				Circle two(sp);
				inters = one.Intof(-turnLeft * this->dir * sp.dir, two);
			}
		}
		if(inters.ok) {
			this->p1 = sp.p0 = inters;
			this->SetProperties(true);
			sp.SetProperties(true);
		}
		return inters.ok;
	}
	static int Split(double tolerance, double angle, double radius, int dir);
	int Span::Split(double tolerance) {
		// returns the number of divisions required to keep in tolerance
		if(returnSpanProperties == false) this->SetProperties(true);
		return geoff_geometry::Split(tolerance, angle, radius, dir);
	}

	int Span3d::Split(double tolerance) {
		// returns the number of divisions required to keep in tolerance
		if(returnSpanProperties == false) this->SetProperties(true);
		return geoff_geometry::Split(tolerance, angle, radius, dir);
	}

	static int Split(double tolerance, double angle, double radius, int dir) {
		if(dir == LINEAR) return 0;	// straight span
		double cosa = 1 - tolerance / radius;
		if(cosa > NEARLY_ONE) cosa = NEARLY_ONE;
		cosa = 2 * cosa * cosa - 1 ;              /* double angle */
		double sina = sqrt(1 - cosa * cosa) * dir;
		double tempang = atan2(sina, cosa);
		int num_vectors = (int)(fabs (angle / tempang )) + 1 ;
		return num_vectors;
	}

	void Span::SplitMatrix(int num_vectors, Matrix* matrix) {
		// returns the incremental matrix
		matrix->Unit();
		if(dir) {
			// arc span
			double incang = angle / (double) num_vectors ;

			matrix->Translate(-pc.x, -pc.y, 0);
			matrix->Rotate(incang, 3);
			matrix->Translate(pc.x, pc.y, 0);
		} else {
			// linear span
			matrix->Translate(length / num_vectors * vs.getx(), length / num_vectors * vs.gety(), 0);
		}
	}

	void Span3d::SplitMatrix(int num_vectors, Matrix* matrix) {
		// returns the incremental matrix
		matrix->Unit();
		if(dir) {
			// arc span
			if(normal.getz() <= NEARLY_ONE) FAILURE(getMessage(L"Unfinished coding - contact the Company", GENERAL_MESSAGES, MES_UNFINISHEDCODING));
			double incang = angle / (double) num_vectors ;

			matrix->Translate(-pc.x, -pc.y, -pc.z);
			matrix->Rotate(incang, 3);
			matrix->Translate(pc.x, pc.y, pc.z);
		} else {
			// linear span
			double d = length / num_vectors;
			matrix->Translate(d * vs.getx(), d * vs.gety(), d * vs.getz());
		}
	}

	void Span::minmax(Box& box, bool start) {
		minmax(box.min, box.max, start);
	}

	void Span3d::minmax(Box3d& box, bool start) {
		minmax(box.min, box.max, start);
	}

	void Span::minmax(Point& min, Point& max, bool start) {
		// box a span (min/max)
		if(start) {
			MinMax(p0, min, max);
		}
		MinMax(p1, min, max);

		if(dir) {
			// check the quadrant points
			double dx1 = p1.x - p0.x;
			double dy1 = p1.y - p0.y;

			double dx = pc.x - p0.x;
			double dy = pc.y - p0.y;

			double dx0 = dx + radius;		// 0deg

			if( dir * (dx0 * dy1 - dx1 * dy) > 0) {
				if(pc.x + radius > max.x) max.x = pc.x + radius;	
			}
			dx0 = dx - radius;				// 180deg
			if( dir * (dx0 * dy1 - dx1 * dy) > 0) {
				if(pc.x - radius < min.x) min.x = pc.x - radius;	
			}
			double dy0 = dy + radius;		// 90deg
			if( dir * (dx * dy1 - dx1 * dy0) > 0) {
				if(pc.y + radius > max.y) max.y = pc.y + radius;	
			}

			dy0 = dy - radius;				// 270deg
			if( dir * (dx * dy1 - dx1 * dy0) > 0) {
				if(pc.y - radius < min.y) min.y = pc.y - radius;	
			}
		}
	}

	void Span3d::minmax(Point3d& min, Point3d& max, bool start) {
		// box a span (min/max)
		if(start) {
			MinMax(p0, min, max);
		}
		MinMax(p1, min, max);

		if(dir) {
			// check the quadrant points ... MUST RECODE THIS FOR 3D sometime
			double dx1 = p1.x - p0.x;
			double dy1 = p1.y - p0.y;

			double dx = pc.x - p0.x;
			double dy = pc.y - p0.y;

			double dx0 = dx + radius;		// 0deg

			if( dir * (dx0 * dy1 - dx1 * dy) > 0) {
				if(pc.x + radius > max.x) max.x = pc.x + radius;	
			}
			dx0 = dx - radius;				// 180deg
			if( dir * (dx0 * dy1 - dx1 * dy) > 0) {
				if(pc.x - radius < min.x) min.x = pc.x - radius;	
			}
			double dy0 = dy + radius;		// 90deg
			if( dir * (dx * dy1 - dx1 * dy0) > 0) {
				if(pc.y + radius > max.y) max.y = pc.y + radius;	
			}

			dy0 = dy - radius;				// 270deg
			if( dir * (dx * dy1 - dx1 * dy0) > 0) {
				if(pc.y - radius < min.y) min.y = pc.y - radius;	
			}
		}
	}

	int Span::Intof(const Span& sp, Point& pInt1, Point& pInt2, double t[4])const {
		// Intof 2 spans
		return geoff_geometry::Intof(*this, sp, pInt1, pInt2, t);
	}

	Point Span::Near(const Point& p)const{
		// returns the near point to span from p 
		if(this->dir == LINEAR) {
			double t;
			t = (Vector2d(this->p0, p) * this->vs);		// t parametised 0 - line length
			return this->vs * t + this->p0;
		} else {
			double r = p.Dist(this->pc);
			if(r < geoff_geometry::TOLERANCE) return (p.Dist(this->p0) < p.Dist(this->p1))?this->p0 : this->p1;
			return(p.Mid(this->pc, (r - this->radius) / r));
		}
	}
	Point Span::NearOn(const Point& p)const{
		// returns the near point to span from p - returned point is always on the span
		Point pn;
		pn = Near(p);
		if(this->OnSpan(pn) == true) return pn;

		// return nearest endpoint
		return (pn.Dist(p0) < pn.Dist(p1))?p0 : p1;
	}

	void Span::Transform(const Matrix& m, bool setprops) {
		p0 = p0.Transform(m);
		p1 = p1.Transform(m);
		if(dir != LINEAR) {
			pc = pc.Transform(m);
			if(m.m_mirrored == -1) FAILURE(L"Don't know mirror - use IsMirrored method on object");
			if(m.m_mirrored) dir = -dir;
		}
		if(setprops == true) SetProperties(true);
	}

	void Span3d::Transform(const Matrix& m, bool setprops) {
		p0 = p0.Transform(m);
		p1 = p1.Transform(m);
		if(dir != LINEAR) {
			pc = pc.Transform(m);
			normal.Transform(m);
			if(m.m_mirrored == -1) FAILURE(L"Don't know mirror - use IsMirrored method on object");
			if(m.m_mirrored) dir = -dir;
		}
		if(setprops == true) SetProperties(true);
	}

	Point Span::Mid()const {
		// midpoint of a span

		return geoff_geometry::Mid(*this);

	}


	Point Span::MidPerim(double d)const {
		/// returns a point which is 0-d along span
		Point p;
		if(this->dir == LINEAR) {
			p = this->vs * d + this->p0;
		}
		else {
			Vector2d v(pc, p0);
			v.Rotate(d * dir / this->radius);
			p = v + pc;
		}
		return p;
	}

	Point Span::MidParam(double param)const {
		/// returns a point which is 0-1 along span
		return MidPerim(param * this->length);
	}

	Vector2d Span::GetVector(double fraction)const {
		/// returns the direction vector at point which is 0-1 along span
		if(dir == 0){
			Vector2d v(p0, p1);
			v.normalise();
			return v;
		}

		Point p = MidParam(fraction);
		Vector2d v(pc, p);
		v.normalise();
		if(dir == ACW)
		{
			return Vector2d(-v.gety(), v.getx());
		}
		else
		{
			return Vector2d(v.gety(), -v.getx());
		}
	}

	Kurve::Kurve(const Kurve& k) :Matrix(){
		/// copy constructor
		this->m_nVertices = k.m_nVertices;
		memcpy(this->e, k.e, 16 * sizeof(double));
		this->m_unit = k.m_unit;
		this->m_mirrored = k.m_mirrored;
		this->m_isReversed = k.m_isReversed;
		this->m_started = k.m_started;
		for(unsigned int i = 0; i < k.m_spans.size(); i++) {
			SpanVertex* spv = new SpanVertex;
			*spv = *k.m_spans[i];
			this->m_spans.push_back(spv);
		}
	}

	const Kurve& Kurve::operator=( const Kurve &k) {
		this->Clear();

		memcpy(e, k.e, 16 * sizeof(double));
		m_unit = k.m_unit;
		m_mirrored = k.m_mirrored;
		m_isReversed = k.m_isReversed;

		if(k.m_nVertices) m_started = true;
//		m_nVertices = 0;

//		spVertex spv;
//		for(int i = 0; i < k.m_nVertices; i++) {
//			k.Get(i, spv);
//			Add(spv);
//		}
		for(unsigned int i = 0; i < k.m_spans.size(); i++) {
			SpanVertex* spv = new SpanVertex;
			*spv = *k.m_spans[i];
			this->m_spans.push_back(spv);
		}
		m_nVertices = k.m_nVertices;
		return *this;
	}

	const Kurve& Kurve::operator=(const Matrix &m)
	{
//		*this = Matrix(m);
//		return *this;

		for(int i = 0; i < 16; i++) e[i] = m.e[i];
		m_unit = m.m_unit;
		m_mirrored = m.m_mirrored;
		return *this;
	}

	Kurve::~Kurve()
	{
		vector<SpanVertex*>::iterator iter = m_spans.begin();
		while(iter != m_spans.end()) {
			delete *iter;
			iter++;
		}
		m_nVertices = 0;
		m_started = m_isReversed = false;
		m_spans.clear();
	}

	bool Kurve::Closed()const
	{
		// returns true if kurve closed
		if(m_nVertices > 1) {
			Point ps, pe, pc;
			Get(0, ps, pc);
			Get(m_nVertices - 1, pe, pc);
			return (ps == pe);
		}
		else
			return false;
	}

	void Kurve::FullCircle(int dir, const Point& c, double radius) {
		/// make a full circle Kurve (2 spans)
		/// mark the first span for later
		this->Clear();
		Point ps = c;
		ps.x = c.x + radius;
		this->Start(ps);
		this->AddSpanID(FULL_CIRCLE_KURVE);
		ps.x = c.x - radius;
		this->Add(dir, ps, c, true);
		ps.x = c.x + radius;
		this->Add(dir, ps, c, true);
	}

	void Kurve::Start()
	{
		if(m_started) this->Clear();
		m_started = true;
	}


	void Kurve::Start(const Point& p)
	{
		Start();
		Add(0, p, Point(0,0));
	}

	bool Kurve::Add(const Span& sp, bool AddNullSpans) {
		// add a span, including ID
		if(this->m_started == false) this->Start(sp.p0);
		if(this->Add(sp.dir, sp.p1, sp.pc, AddNullSpans)) {
			this->AddSpanID(sp.ID);
			return true;
		}
		return false;
	}

	bool Kurve::Add(const spVertex& spv, bool AddNullSpans) {
		if(Add(spv.type, spv.p, spv.pc, AddNullSpans)) {
			AddSpanID(spv.spanid);
			return true;
		}
		return false;
	}

	bool Kurve::Add(int span_type, const Point& p0, const Point& pc, bool AddNullSpans)
	{
		// add a span (cw = -1 (T)   acw = 1 (A) )
#ifdef _DEBUG
		if(this == NULL) FAILURE(L"Kurve::Add - No Kurve Object");
#endif

		if(!m_started) {
			Start(p0);
			return true;
		}

		if(m_nVertices) {
			// see if a null span would result by the addition of this span
			//		double xl, yl, cxl, cyl;
			Point pv, pcc;
			Get(m_nVertices - 1, pv, pcc);
			if(pv.Dist(p0) < geoff_geometry::TOLERANCE) {
				if(!AddNullSpans)return false;
				span_type = LINEAR;				// linear span
			}
		}

		SpanVertex* p;
		if(m_nVertices % SPANSTORAGE == 0) {
			p = new SpanVertex;
			m_spans.push_back(p);
		}
		else 
			p = (SpanVertex*) m_spans[m_nVertices / SPANSTORAGE];

		p->Add(m_nVertices % SPANSTORAGE, span_type, p0, pc);
		m_nVertices++;
		return true;
	}
	void Kurve::AddSpanID(int ID)
	{
		// add a extra data - must be called after Add
		int vertex = this->m_nVertices - 1;
		SpanVertex* p = (SpanVertex*) m_spans[vertex / SPANSTORAGE];
		p->AddSpanID(vertex % SPANSTORAGE, ID);
	}

	void Kurve::Add() {
		// null span
		if(m_nVertices == 0) FAILURE(L"Invalid attempt to add null span - no start");
		Point p, pc;
		Get(m_nVertices - 1, p, pc);
		Add(p, true);
	}

	bool Kurve::Add(const Point& p0, bool AddNullSpans) {
		return Add(0, p0, Point(0,0), AddNullSpans);
	}


	void Kurve::Add(const Kurve* k, bool AddNullSpans) {
		Span sp;
		Matrix m;
		if(this->m_unit == false) {
			m = *k;
			Matrix im = this->Inverse();
			m.Multiply(im);
			m.IsUnit();
		}
		for(int i = 1; i <= k->nSpans(); i++) {
			k->Get(i, sp, false, this->m_unit);
			#ifndef PEPSDLL
			const SpanDataObject* obj = k->GetIndex(i-1);
			#endif
			if(this->m_unit == false) sp.Transform(m);

			if(i == 1) {
				// check if this is the same as last point in kurve
				bool AddFirstVertex = true;
				if(nSpans()) {
					Span spLast;
					Get(nSpans(), spLast, false, false);
					if(spLast.p1.Dist(sp.p0) <= geoff_geometry::TOLERANCE) AddFirstVertex = false;
				}
				if(AddFirstVertex) { 
					Add(sp.p0, AddNullSpans);
					#ifndef PEPSDLL
					if(obj != NULL) {
						SpanDataObject* objnew = new SpanDataObject(obj);
						AddIndex(nSpans() - 1, objnew);
					}
					#endif
				}
			}
			
			Add(sp.dir, sp.p1, sp.pc, AddNullSpans);
			#ifndef PEPSDLL
				if(obj != NULL) {
					SpanDataObject* objnew = new SpanDataObject(obj);
					AddIndex(nSpans() - 1, objnew);
			}
			#endif
		}
	}

	void Kurve::Replace(int vertexnumber, const spVertex& spv) {
		// replace a span
		Replace(vertexnumber, spv.type, spv.p, spv.pc, spv.spanid);
	}


	void Kurve::Replace(int vertexnumber, int type, const Point& p0, const Point& pc, int ID) {
		// replace a span
#ifdef _DEBUG
		if(this == NULL || vertexnumber > m_nVertices) FAILURE(getMessage(L"Kurve::Replace - vertexNumber out of range", GEOMETRY_ERROR_MESSAGES, MES_BAD_VERTEX_NUMBER));
#endif
		SpanVertex* p = (SpanVertex*) m_spans[vertexnumber / SPANSTORAGE];
		p->Add(vertexnumber % SPANSTORAGE, type, p0, pc, ID);
	}

#ifdef PEPSDLL
	void Kurve::ModifyIndex(int vertexnumber, WireExtraData* i) {
		// replace an index
#ifdef _DEBUG
		if(this == NULL || vertexnumber > m_nVertices) FAILURE(getMessage(L"Kurve::ModifyIndex - vertexNumber out of range", GEOMETRY_ERROR_MESSAGES, MES_BAD_VERTEX_NUMBER));
#endif
		SpanVertex* p = (SpanVertex*) m_spans[vertexnumber / SPANSTORAGE];
		p->Add(vertexnumber % SPANSTORAGE, i);
	}
#else
	void Kurve::AddIndex(int vertexNumber, const SpanDataObject* data) {
		if(this == NULL || vertexNumber > m_nVertices - 1) FAILURE(L"Kurve::AddIndex - vertexNumber out of range");
		SpanVertex* p = (SpanVertex*) m_spans[vertexNumber / SPANSTORAGE];
		p->Add(vertexNumber % SPANSTORAGE, data);
	}

	const SpanDataObject* Kurve::GetIndex(int vertexNumber)const {
		if(this == NULL || vertexNumber > m_nVertices - 1) FAILURE(L"Kurve::GetIndex - vertexNumber out of range");
		SpanVertex* p = (SpanVertex*) m_spans[vertexNumber / SPANSTORAGE];
		return p->GetIndex(vertexNumber % SPANSTORAGE);
	}


#endif
	void Kurve::Get(std::vector<Span> *all, bool igNoreNullSpans)const {
		/// put all spans to vector
		for(int i = 1; i <= nSpans(); i++) {
			Span sp;
			Get(i, sp, true);
			if(igNoreNullSpans == true && sp.NullSpan == true) continue;
			all->push_back(sp);
		}
	}

	void Kurve::Get(int vertexnumber, spVertex& spv) const {
		spv.type = Get(vertexnumber, spv.p, spv.pc);
		spv.spanid = GetSpanID(vertexnumber);
	}

	int	Kurve::Get(int vertexnumber, Point& pe, Point& pc) const {
		// returns spantype with end / centre by reference
		if(vertexnumber < 0 || vertexnumber >= m_nVertices) FAILURE(getMessage(L"Kurve::Get - vertexNumber out of range", GEOMETRY_ERROR_MESSAGES, MES_BAD_VERTEX_NUMBER));
		if(m_isReversed == true) {
			int revVertexnumber = m_nVertices - 1 - vertexnumber;
			SpanVertex* p = (SpanVertex*)m_spans[revVertexnumber / SPANSTORAGE];
			int offset = revVertexnumber % SPANSTORAGE;
			pe = Point(p->x[offset], p->y[offset]);
			if(vertexnumber > 0) {
				revVertexnumber++;
				offset = revVertexnumber % SPANSTORAGE;
				p = (SpanVertex*)m_spans[revVertexnumber / SPANSTORAGE];
				pc = Point(p->xc[offset], p->yc[offset]);
				return -p->type[offset];
			}
			else return LINEAR;
		}
		else {
			SpanVertex* p = (SpanVertex*)m_spans[vertexnumber / SPANSTORAGE];
			return p->Get(vertexnumber % SPANSTORAGE, pe, pc);
		}
	}
	int	Kurve::GetSpanID(int vertexnumber) const {
		// for spanID (wire offset)
		if(vertexnumber < 0 || vertexnumber >= m_nVertices) FAILURE(getMessage(L"Kurve::Get - vertexNumber out of range", GEOMETRY_ERROR_MESSAGES, MES_BAD_VERTEX_NUMBER));
		if(m_isReversed == true) vertexnumber = m_nVertices - 1 - vertexnumber;
		SpanVertex* p = (SpanVertex*)m_spans[vertexnumber / SPANSTORAGE];
		return p->GetSpanID(vertexnumber % SPANSTORAGE);
	}
	int Kurve::Get(int spannumber, Span& sp, bool returnSpanProperties, bool transform) const {
		// returns span data and optional properties - the function returns as the span type
		if(spannumber < 1 || spannumber > m_nVertices) FAILURE(getMessage(L"Kurve::Get - vertexNumber out of range", GEOMETRY_ERROR_MESSAGES, MES_BAD_VERTEX_NUMBER));
		if(m_nVertices < 2) return -99;

		int spanVertexNumber = spannumber - 1;
		if(m_isReversed) spanVertexNumber = m_nVertices - 1 - spanVertexNumber;
		SpanVertex* p = (SpanVertex*)m_spans[spanVertexNumber / SPANSTORAGE];
		sp.p0.x = p->x[spanVertexNumber % SPANSTORAGE];
		sp.p0.y = p->y[spanVertexNumber % SPANSTORAGE];
		sp.p0.ok = 1;

		sp.dir = Get(spannumber, sp.p1, sp.pc);
		sp.ID = GetSpanID(spannumber);

		if(transform && !m_unit) {
			const Matrix *m = this;
			sp.Transform(*m, false);
		}

		sp.SetProperties(returnSpanProperties);

		return sp.dir;
	}

	int Kurve::Get(int spannumber, Span3d& sp, bool returnSpanProperties, bool transform) const {
		// returns span data and optional properties - the function returns as the span type
		if(spannumber < 1 || spannumber > m_nVertices) FAILURE(getMessage(L"Kurve::Get - vertexNumber out of range", GEOMETRY_ERROR_MESSAGES, MES_BAD_VERTEX_NUMBER));
		if(m_nVertices < 2) return -99;

		int spanVertexNumber = spannumber - 1;
		SpanVertex* p = (SpanVertex*)m_spans[spanVertexNumber / SPANSTORAGE];
		sp.p0.x = p->x[spanVertexNumber % SPANSTORAGE];
		sp.p0.y = p->y[spanVertexNumber % SPANSTORAGE];
		sp.p0.z = 0;
//		sp.p0.ok = 1;

		sp.dir = Get(spannumber, sp.p1, sp.pc);

		if(transform && !m_unit) {
			const Matrix *m = this;
			sp.Transform(*m, false);
		}

		sp.SetProperties(returnSpanProperties);

		return sp.dir;
	}

	void Kurve::Get(Point &ps,Point &pe) const
	{
		// returns the start- and endpoint of the kurve
		Span sp;
		Get(1,sp,true,true);
		ps = sp.p0;
		Get(m_nVertices-1,sp,true,true);
		pe = sp.p1;
	}

	void Span::SetProperties(bool returnProperties) {
		if(returnSpanProperties = returnProperties) {
			// return span properties
			if(dir) {
				// arc properties
				vs = ~Vector2d(pc, p0);		// tangent at start ( perp to radial vector)
				ve = ~Vector2d(pc, p1);		// tangent at end   ( perp to radial vector)
				if(dir == CW) {
					vs = -vs;				// reverse directions for CW arc
					ve = -ve;
				}

				radius = vs.normalise();
				double radCheck = ve.normalise();
//				if(FNE(radius, radCheck, geoff_geometry::TOLERANCE * 0.5)){
				if(FNE(radius, radCheck, geoff_geometry::TOLERANCE)){
					FAILURE(getMessage(L"Invalid Geometry - Radii mismatch - SetProperties", GEOMETRY_ERROR_MESSAGES, MES_INVALIDARC));
				}
				
				length = 0.0;
				angle = 0.0;
				if(radius > geoff_geometry::TOLERANCE) {
					if(NullSpan = (p0.Dist(p1) <= geoff_geometry::TOLERANCE)) {
						dir = LINEAR;
					}
					else {
						// arc length & included angle
						length = fabs(angle = IncludedAngle(vs, ve, dir)) * radius;
					}
				}
				else
					NullSpan = true;
			}
			else {
				// straight properties
				vs = Vector2d(p0, p1);

				length = vs.normalise();
				NullSpan = (length <= geoff_geometry::TOLERANCE);
				ve = vs;
			}
			minmax(box, true);
		}
	}

	void Span3d::SetProperties(bool returnProperties) {
		if(returnSpanProperties = returnProperties) {
			// return span properties
			if(dir) {
				// arc properties
				vs = normal ^ Vector3d(pc, p0);// tangent at start ( perp to radial vector)
				ve = normal ^ Vector3d(pc, p1);// tangent at end   ( perp to radial vector)
				if(dir == CW) {
					vs = -vs;				// reverse directions for CW arc
					ve = -ve;
				}

				radius = vs.normalise();
				double radCheck = ve.normalise();
				if(FNE(radius, radCheck, geoff_geometry::TOLERANCE * 0.5)) FAILURE(getMessage(L"Invalid Geometry - Radii mismatch - SetProperties", GEOMETRY_ERROR_MESSAGES, MES_INVALIDARC));
				if(radius > geoff_geometry::TOLERANCE) {
					if(NullSpan = (p0.Dist(p1) <= geoff_geometry::TOLERANCE)) {
						length = 0.0;
						angle = 0.0;
						dir = LINEAR;
					}
					else {
						// arc length & included angle
						length = fabs(angle = IncludedAngle(vs, ve, normal, dir)) * radius;
					}
				}
				else
					NullSpan = true;
			}
			else {
				// straight properties
				vs = Vector3d(p0, p1);

				length = vs.normalise();
				NullSpan = (length <= geoff_geometry::TOLERANCE);
				ve = vs;
			}
			minmax(box, true);
		}
	}


	Point Mid(const Span& span) {
		// mid point of a span
		if(span.dir) {
			CLine chord(span.p0, span.p1);
			if(chord.ok) {
				CLine bisector(Mid(span.p0, span.p1), ~chord.v, false);
				return Intof((span.dir == CW) ?FARINT : NEARINT, bisector, Circle(span));
			}
			else
				return span.p0;
		}
		else
			return Mid(span.p0, span.p1);
	}

	Point Kurve::Near(const Point& p, int& nearSpanNumber)const {
		// finds the nearest span on kurve to the the given point, nearSpanNumber is the spannumber
		double minDist = 1.0e100;
		Point pNear, pn;

		nearSpanNumber = 0;
		for(int i = 1; i <= nSpans(); i++) {
			Span sp;
			Get(i, sp, true, true);
			pNear = sp.NearOn(p);
			double d = pNear.Dist(p);
			if(minDist > d) {
				nearSpanNumber = i;
				pn = pNear;
				minDist = d;
				if(minDist < geoff_geometry::TOLERANCE) break;		// p must be on the span
			}
		}
		return pn;
	}


	Point Kurve::NearToVertex(const Point& p, int& nearSpanNumber)const {
		// finds the nearest span endpoint on kurve to the the given point, nearSpanNumber is the spannumber
		double minDistSquared = 1.0e100;
		Point pn;

		Matrix inv_mat = *this;
		inv_mat.Inverse();
		
		Point tp = p;
		if(!m_unit)	tp = tp.Transform(inv_mat); // Inverse transform point (rather than transform each vertex!)

		nearSpanNumber = 0;

		for(int i = 0; i < m_nVertices; i++) {
			Point ps, pc;
			Get(i, ps, pc);
			double DistSquared = Vector2d(ps, tp).magnitudesqd();
			if(DistSquared < minDistSquared) {
				minDistSquared = DistSquared;
				nearSpanNumber = i;
				pn = ps;
			}
		}
		return pn.Transform(*this);
	}

	void Kurve::ChangeStart(const Point *pNewStart, int startSpanno) {
		// changes the start position of the Kurve
		if(startSpanno == 1) {
			Span spFirst;
			this->Get(1, spFirst, false, true);
			if(spFirst.p0 == *pNewStart) return;
		}
		else if(startSpanno == this->nSpans()) {
			Span spLast;
			this->Get(this->nSpans(), spLast, false, true);
			if(spLast.p1 == *pNewStart) return;
		}
		Kurve temp;
		
		bool wrapped = false;
		int nSpans = 0;
		int spanno = startSpanno;
		Span sp;
		while(1) {
			this->Get(spanno, sp, false, true);
			if(spanno == startSpanno && wrapped == false) {
				temp.Start(*pNewStart);
				temp.Add(sp.dir, sp.p1, sp.pc, true);
			}
			else {
				if(nSpans == this->nSpans() && this->Closed() == true) {
					sp.p1 = *pNewStart;
				}
				temp.Add(sp, true);
			}

			spanno++;
			nSpans++;
			if(nSpans > this->nSpans()) break;
			if(spanno > this->nSpans()) {
				if(this->Closed() == false) break;
				spanno = 1;
				wrapped = true;
			}
		}

		*this = temp;
	}


	void Kurve::minmax(Point& min, Point& max) {
		// boxes kurve
		double xscale = 1.0;
		min = Point(1.0e61, 1.0e61);
		max = Point(-1.0e61, -1.0e61);

		if(!GetScale(xscale)) FAILURE(getMessage(L"Differential Scale not allowed for this method", GEOMETRY_ERROR_MESSAGES, MES_DIFFSCALE));	// differential scale
		Span sp;
		for(int i = 1; i < m_nVertices; i++) {
			Get(i, sp, true, true);
			if(i == 1) MinMax(sp.p0, min, max);
			sp.minmax(min, max, false);
		}
	}

	void	Kurve::minmax(Box& b) {
		minmax(b.min, b.max);
	}

	void Kurve::StoreAllSpans(std::vector<Span>& kSpans)const {	// store all kurve spans in array, normally when fast access is reqd
		Span span;
		for(int i = 1; i <= this->nSpans(); i++) {
			this->Get(i, span, true, false);					
			kSpans.push_back(span);
		}
	}

	void Kurve::Clear()
	{
		for(vector<SpanVertex*>::iterator It = m_spans.begin(); It != m_spans.end(); It++)
		{
			SpanVertex* spv = *It;
			delete spv;
		}
		m_spans.clear();
		m_started = false;
		m_nVertices = 0;
		m_isReversed = false;
	}

	bool Kurve::operator==(const Kurve &k)const{
		// k = kk (vertex check)
		if(nSpans() != k.nSpans()) return false;
		spVertex thisvertex, vertex;
		for(int i = 0; i <= nSpans(); i++) {
			this->Get(i, thisvertex);
			k.Get(i, vertex);
			if(thisvertex != vertex) return false;
		}
		return true;
	}

#ifndef OPEN_SOURCE_GEOMETRY
	void Kurve::read(FILE* d, double scale) {
		// debug - read next kurve in peps command input format
		// scale is for unit conversion (not using matrix for better accuracy)
		wchar_t line[1024];
		spVertex spv;
		while(1) {
			if(readLine(d, line, 1024) == EOF) break;
			wchar_t *pline = line;
			lowercase(line);

			pline = stripLeadingWhiteSpace(pline);

			switch(line[0]) {

				case 'x':
					pline = getNextAddressDouble('x', pline, &spv.p.x);
					pline = getNextAddressDouble('y', pline, &spv.p.y);
					spv.type = LINEAR;
					spv.p.x *= scale;
					spv.p.y *= scale;
					this->Add(spv);
					break;

				case 'a':
				case 't':
					spv.type = (*pline == 'a')?ACW : CW;
					pline++;
					pline = getNextAddressDouble('x', pline, &spv.p.x);
					pline = getNextAddressDouble('y', pline, &spv.p.y);
					pline = getNextAddressDouble('x', pline, &spv.pc.x);
					pline = getNextAddressDouble('y', pline, &spv.pc.y);
					spv.p.x *= scale;
					spv.p.y *= scale;
					spv.pc.x *= scale;
					spv.pc.y *= scale;

					this->Add(spv);

					break;

				case 'e':
					return;

			}

		}
		return;
	}

	int readLine(FILE* d, wchar_t* line, int maxCount){
		// read a line
		int i = 0;
		while(i < maxCount) {
			int ch = fgetc(d);
			if(ch == EOF) return EOF;
			line[i++] = ch;
			if(ch == '\n') {
				line[i] = 0;
				return 0;
			}
		}
		return EOF;
	}


	char* stripLeadingWhiteSpace(char* line) {
		// strip leading white space from line
		char *ptr = line;
		while(1) {
			if(*ptr == ' ' || *ptr == '\t') {
				ptr++;
			}
			else
				break;
		}
		return ptr;
	}

	wchar_t* stripLeadingWhiteSpace(wchar_t* line) {
		// strip leading white space from line
		wchar_t *ptr = line;
		while(1) {
			if(*ptr == ' ' || *ptr == '\t') {
				ptr++;
			}
			else
				break;
		}
		return ptr;
	}

	void stripTrailingWhiteSpace(wchar_t* line) {
		wchar_t *ptr = line + wcslen(line) - 1;
		while(ptr > line) {
			if(*ptr == ' ' || *ptr == '\t') *ptr = 0;
			else break;
			ptr--;
		}
	}

	void stripTrailingWhiteSpace(char* line) {
		char *ptr = line + strlen(line) - 1;
		while(ptr > line) {
			if(*ptr == ' ' || *ptr == '\t') *ptr = 0;
			else break;
			ptr--;
		}
	}

	wchar_t *lowercase( wchar_t* str ) {
		wchar_t ch, *ptr = str;
		while ((ch = *ptr) != 0)
		{
			if (isupper(ch)) *ptr = tolower( ch );
			++ptr;
		}
		return str;
    }

	wchar_t* getNextWord(wchar_t* str, wchar_t* word) {
		wchar_t *ptr = stripLeadingWhiteSpace(str);
		while(*ptr != 0 && *ptr != ' ' && *ptr != '\n') {
			*word = *ptr;
			word++;
			ptr++;
		}
		*word = 0;
		return ptr;
	}

	wchar_t *getNextString(wchar_t* str, wchar_t* string) {
		wchar_t *ptr = stripLeadingWhiteSpace(str);
		if(*ptr != '\'') return getNextWord(ptr, string);
		ptr++;
		while(*ptr != '\'') {
			*string = *ptr;
			if(*ptr == 0 || *ptr == '\n') break;
			string++;
			ptr++;
		}
		*string = 0;
		return ++ptr;
	}


	wchar_t *getNextCommand(wchar_t* str, wchar_t* word) {
		// command may be terminated by a numeric
		wchar_t *ptr = stripLeadingWhiteSpace(str);
		while(*ptr >= 'A') {
			*word = *ptr;
			word++;
			ptr++;
		}
		*word = 0;
		return ptr;
	}

	wchar_t *getNextDouble(wchar_t* str, double* value) {
		wchar_t word[1024];
		wchar_t *ptr = getNextWord(str, word);
		*value = _wtof(word);
		return ptr;
	}

	wchar_t *getNextInt(wchar_t* str, int* value) {
		wchar_t word[1024];
		wchar_t *ptr = getNextWord(str, word);
		*value = _wtoi(word);
		return ptr;
	}

	wchar_t *getNextAddressDouble(wchar_t address, wchar_t* str, double* value) {
		wchar_t *ptr = stripLeadingWhiteSpace(str);
		if(*ptr != address) FAILURE(L"wrong address");
		ptr++;

		ptr = getNextDouble(ptr, value);
		return ptr;
	}

	wchar_t *getNextAddressInt(wchar_t address, wchar_t* str, int* value) {
		wchar_t *ptr = stripLeadingWhiteSpace(str);
		if(*ptr != address) FAILURE(L"wrong address");
		ptr++;

		ptr = getNextInt(ptr, value);
		return ptr;
	}

	wchar_t *skipOver(wchar_t* str, wchar_t ch) {
		wchar_t *ptr = str;
		while(*ptr != 0 && *ptr != '\n') {
			if(*ptr == ch) return ++ptr;
			ptr++;
		}
		return str;
	}

	void changeFileExtension(std::wstring& fname, wchar_t *ext) {

		// locate previous extension
		std::wstring::size_type p = fname.rfind('.');
		if(p != std::wstring::npos)	{
			fname.resize(p);
		}
		fname.append(ext);
	}


	wofstream& operator << (wofstream& op, Kurve& k) {
		// debug - output in peps command input format
		// untransformed ??
		for(int i = 0; i < k.m_nVertices; i++) {
			Point p, pc;
			int dir = k.Get(i, p, pc);

			if(dir == LINEAR)
				op << L"x" << p.x << L" y" << p.y << '\n';
			else
				op << ((dir == ACW)?'a' : 't') << L"x" << p.x << L" y" << p.y << L" x" << pc.x << L" y" << pc.y << '\n';
		}
		op << L"ek\n";
		return op;
	}

		void	Kurve::printXML(FILE* d, int kid) {
		// debug - output in peps command input format
		fprintf(d,"<Kurve id = \"%d\">\n", kid);
		for(int i = 0; i < this->m_nVertices; i++) {
			Point p, pc;
			int dir = this->Get(i, p, pc);

			if(dir == LINEAR)
				fprintf(d, "<LINEAR x = \"%.14g\" y = \"%.14g\"/>\n", p.x, p.y);
			else {
				if(dir == ACW)
					fprintf(d, "<ACW x = \"%.14g\" y = \"%.14g\" xc = \"%.14g\" yc = \"%.14g\"/>\n", p.x, p.y, pc.x, pc.y);
				else
					fprintf(d, "<CW x = \"%.14g\" y = \"%.14g\" xc = \"%.14g\" yc = \"%.14g\"/>\n", p.x, p.y, pc.x, pc.y);
			}
		}
		fprintf(d, "</Kurve>\n");
	}
#endif

	double Kurve::Perim() const{
		// returns perimeter of kurve
		double perim = 0;
		Span sp;
		double xscale = 1.0;
		if(!GetScale(xscale)) FAILURE(getMessage(L"Differential Scale not allowed for this method", GEOMETRY_ERROR_MESSAGES, MES_DIFFSCALE));	// differential scale

		if(m_nVertices > 1) {
			for(int i = 1; i < m_nVertices; i++)
				perim += (Get(i, sp, true))? fabs(sp.angle) * sp.radius : sp.length;
		}
		return perim * xscale;
	}
	double Kurve::Area() const{
		// returns Area of kurve (+ve clockwise , -ve anti-clockwise sense)
		double xscale = 1.0;
		double area = 0;
		Span sp;

		if(Closed()) {
			if(!GetScale(xscale)) FAILURE(getMessage(L"Differential Scale not allowed for this method", GEOMETRY_ERROR_MESSAGES, MES_DIFFSCALE));	// differential scale
			for(int i = 1; i < m_nVertices; i++) {			
				if(Get(i, sp, true))
					area += ( 0.5 * ((sp.pc.x - sp.p0.x) * (sp.pc.y + sp.p0.y) - (sp.pc.x - sp.p1.x) * (sp.pc.y + sp.p1.y) - sp.angle * sp.radius * sp.radius));
				else
					area += 0.5 * (sp.p1.x - sp.p0.x) * (sp.p0.y + sp.p1.y);
			}
		}
		return area * xscale * xscale;
	}

	static void bubblesort(vector<Point>&p, vector<double>& d);

	int Kurve::Intof(const Span& spin, vector<Point>& p)const {
		// returns a vector (array) of intersection points
		int totalPoints = 0;
		for(int i = 1; i <= nSpans(); i++) {
			Span sp;
			Get(i, sp, true, true);

			Point pInt1, pInt2;
			double t[4];
			int numint = sp.Intof(spin, pInt1, pInt2, t);
			if(numint)		p.push_back(pInt1);
			if(numint == 2)	p.push_back(pInt2);
			totalPoints += numint;
		}
		if(totalPoints) {
			// sort intersects along span
			vector<double> d;
			Span temp(spin);

			for(int i = 0; i < (int)p.size(); i++) {
				temp.p1 = p[i];
				temp.SetProperties(true);

				d.push_back(temp.length);
			}
			bubblesort(p, d);
		}
		return totalPoints;
	}

	static void bubblesort(vector<Point>&p, vector<double>& d) {

		for(int pass = 1; pass < (int)p.size() ; pass++) {
			for(int j = 0; j < (int)p.size() - 1; j++) {
				if(d[j] > d[j+1] ) {
					// swap
					Point temp = p[j];
					p[j] = p[j+1];
					p[j+1] = temp;
					double dtemp = d[j];
					d[j] = d[j+1];
					d[j+1] = dtemp;
				}
			}
		}
	}

	int Kurve::Intof(const Kurve&k, vector<Point>& p)const {
		vector<Point> all;

		int totalPoints = 0;
		for(int i = 1; i <= nSpans(); i++) {
			Span sp;
			Get(i, sp, true, true);
			vector<Point> p0;
			totalPoints += k.Intof(sp, p0);

			for(int j = 0; j < (int)p0.size(); j++) all.push_back(p0[j]);
		}
		//FILE* d;
		//d = fopen("\\temp\\test.txt", "w");
		//		for(int l = 0; l < all.size(); l++) all[l].print(d, "all","\n");


		for(int i = 0; i < (int)all.size(); i++) {
			if(i == 0) 
				p.push_back(all[0]);
			else
				if(all[i-1].Dist(all[i]) > geoff_geometry::TOLERANCE) p.push_back(all[i]);
		}

		//fclose(d);
		return (int)p.size();
	}

	bool Kurve::Split(double MaximumRadius, double resolution) {

		Span sp;
		bool changed = false;
		Kurve ko;

		Get(0, sp.p0, sp.pc);
		ko.Start(sp.p0);

		for(int i = 1 ; i < m_nVertices; i++) {
			sp.dir = Get(i, sp.p1, sp.pc);
			if(sp.dir) {
				sp.SetProperties(true);
				if(sp.radius >= MaximumRadius) {
					// split this arc
					int nSplits = sp.Split(resolution);
					if(nSplits > 1) {
						Matrix m;
						sp.SplitMatrix(nSplits, &m);
						for(int j = 1; j < nSplits; j++) {
							sp.p0 = sp.p0.Transform(m);
							ko.Add(sp.p0);	
						}

						sp.dir = LINEAR;
						changed = true;
					}
				}
			}

			ko.Add(sp.dir, sp.p1, sp.pc);

			sp.p0 = sp.p1;

		}		
		// copy kurve
		if(changed) *this = ko;
		return changed;
	}

	void Kurve::Reverse() {
		// reverse the direction of a kurve
		int nSwaps = (m_nVertices - 1) / 2;
		if(nSwaps == 0) return;
		Point p0, pc0;			// near
		Point pend, pcend;	// far

		int i = 0, j = m_nVertices - 1;
		int dir0 = Get(i, p0, pc0);
		int spanID0 = GetSpanID(i);
		int dirend = Get(j, pend, pcend);
		int spanIDend = GetSpanID(j);

		while(i <= nSwaps) {
			Point p1, pc1;
			int dir1 = Get(i+1, p1, pc1);
			int spanID1 = GetSpanID(i+1);
			Point pendp, pcendp;	// far previous
			int direndp = Get(j-1, pendp, pcendp);
			int spanIDendp = GetSpanID(j-1);
			// end point
			Replace(i, dir0, pend, pc0, spanID0);
			Replace(j, dirend, p0, pcend, spanIDend);

			dir0 = dir1;
			p0 = p1;
			pc0 = pc1;
			dirend = direndp;
			pend = pendp;
			pcend = pcendp;
			spanID0 = spanID1;
			spanIDend = spanIDendp;

			i++;
			j--;
		}

		// now circle data - but it should be easy to modify centre data in the loop above (for another day)
		i = 0;
		j = m_nVertices - 1;
		dir0 = Get(i, p0, pc0);
		dirend = Get(j, pend, pcend);

		while(i < nSwaps) {
			Point p1, pc1;
			Point pendp, pcendp;	// far previous

			int dir1 = Get(i+1, p1, pc1);
			int direndp = Get(j-1, pendp, pcendp);

			Replace(i+1, -dirend, p1, pcend);
			Replace(j, -dir1, pend, pc1);
			dir0 = dir1;
			p0 = p1;
			pc0 = pc1;
			dirend = direndp;
			pend = pendp;
			pcend = pcendp;
			i++;
			j--;
		}
	}

	int Kurve::Reduce(double tolerance) {
		// remove spans that lie within tolerance
		// returns the number of spans removed
		if(nSpans() <= 2) return 0;								// too few spans for this method
		Kurve kReduced;
		kReduced = Matrix(*this);

#if 0
		for(int i = 1; i <= this->nSpans(); i++) {
			Span sp;
			this->Get(i, sp, true);

			for(int j = i+1; j <= this->nSpans(); j++) {
				Span spnext;
				this->Get(j, spnext, true);

			}
		}
		return m_nVertices - kReduced.m_nVertices;

#else 
		int dir1, dir2;
		Point p0, p1, p2, pc0, pc1, pc2;
		int vertex = 0;
		int dir0 = Get(vertex++, p0, pc0);		// first vertex
		kReduced.Start(p0);
		int lvertex = vertex++;

		while(vertex < m_nVertices) {
			while(vertex < m_nVertices) {
				int savelvertex = lvertex;
				int addvertex = vertex - 1;
				dir2 = Get(vertex++, p2, pc2);
				CLine cl(p0, p2);
				if(cl.ok) {
					bool outoftol = false;
					while(lvertex < vertex - 1) {									// interior loop, p1 after p0 up to vertex before p2
						dir1 = Get(lvertex++, p1, pc1);

						if(dir1 || fabs(cl.Dist(p1)) > tolerance) {
							outoftol = true;
							break;
						}
					}
					if(outoftol) {
						dir0 = Get(addvertex, p0, pc0);
						kReduced.Add(dir0, p0, pc0);
						lvertex = addvertex + 1;
					}
					else {
						lvertex = savelvertex;
					}
				}
			}
		}
		kReduced.Add(dir2, p2, pc2);

		if(m_nVertices != kReduced.m_nVertices) *this = kReduced;
		return m_nVertices - kReduced.m_nVertices;
#endif
	}

void Kurve::Part(int startVertex, int EndVertex, Kurve *part) {
	// make a part kurve
	spVertex spv;
	for(int i = startVertex; i <= EndVertex; i++) {
		Get(i, spv);
		part->Add(spv, true);
	}
	return;
}


Kurve Kurve::Part(int fromSpanno, const Point& fromPt, int toSpanno, const Point& toPt) {
            // make a Part Kurve
            // if spanno are known containing from/to Points then this is used, otherwise set = 0
            Kurve kPart;
            Span span;
            Point ps,pe;
            int iStartSpanNr,iEndSpanNr,i;
            
            // get start point and start spannumber
            if(fromSpanno == 0)
                  ps = Near(fromPt,iStartSpanNr);
            else
            {
                  Get(fromSpanno,span,true,true);
                  ps = span.p0;
                  iStartSpanNr = fromSpanno;
            }
            // get end point and end spannumber
            if(toSpanno == 0)
                  pe = Near(toPt,iEndSpanNr);
            else
            {
                  Get(toSpanno,span,true,true);
                  pe = span.p1;
                  iEndSpanNr = toSpanno;
            }
            
            kPart.Start(ps);
            Get(iStartSpanNr,span,true,true);

            if(iStartSpanNr == iEndSpanNr)
            {
                  kPart.Add(span.dir,pe,span.pc);
                  return kPart;
            }

            if(iStartSpanNr < iEndSpanNr)
            {
                  for(i=iStartSpanNr;i<iEndSpanNr;i++)
                  {
                        Get(i,span,true,true);
                        kPart.Add(span.dir,span.p1,span.pc);
                  }
                  Get(iEndSpanNr,span,true,true);
                  kPart.Add(span.dir,pe,span.pc);
            }
            if(iStartSpanNr > iEndSpanNr)
            {
                  for(i=iStartSpanNr;i<=nSpans();i++)
                  {
                        Get(i,span,true,true);
                        kPart.Add(span.dir,span.p1,span.pc);
                  }
                  if(Closed() == false)
                  {
                        Get(1,span,true,true);
                        kPart.Add(0,span.p0,Point(0.0,0.0)); // Add new span from kend to kstart
                  }
                  for(i=1;i<iEndSpanNr;i++)
                  {
                        Get(i,span,true,true);
                        kPart.Add(span.dir,span.p1,span.pc);
                  }
                  Get(iEndSpanNr,span,true,true);
                  kPart.Add(span.dir,pe,span.pc);
            }
            return kPart;
      }

#ifndef OPEN_SOURCE_GEOMETRY
	void	Kurve::print(FILE* d, int kid, bool outSpanid) {
		// debug - output in peps command input format
		// untransformed ??
		fprintf(d,"k%d e\n", kid);
		for(int i = 0; i < this->m_nVertices; i++) {
			Point p, pc;
			int dir = this->Get(i, p, pc);
			if(outSpanid) {
			int SpanID = this->GetSpanID(i);
			std::wstring spanid;
				switch(SpanID) {
					case UNMARKED:
						spanid = L"UNMARKED";
						break;
					case ROLL_AROUND:
						spanid = L"ROLL_AROUND RADIUS";
						break;
					case INTERSECTION:
						spanid = L"INTERSECTION";
						break;

					default:
						spanid = L"UNSET";
						break;
				}
				if(dir == LINEAR)
					fwprintf(d, L"x%.14g y%.14g; '*** ID = %s\n", p.x, p.y, spanid);
				else
					fwprintf(d, L"%cx%.14g y%.14g x%.14g y%.14g; '*** ID = %s\n", (dir == ACW)?'a' : 't', p.x, p.y, pc.x, pc.y, spanid);
			}
			else {
				if(dir == LINEAR)
					fwprintf(d, L"x%.14g y%.14g\n", p.x, p.y);
				else
					fwprintf(d, L"%cx%.14g y%.14g x%.14g y%.14g\n", (dir == ACW)?'a' : 't', p.x, p.y, pc.x, pc.y);
			}
		}
		fprintf(d,"ek\n");
	}
#endif

	Kurve Kurve::Part(double fromParam, double toParam) {
		/// return a part Kurve - perimeter parameterisation
		/// fromParam & toParam 0 - 1 perimeter parameter
		Kurve k;

		double perimTotal = this->Perim();
		double fromPerim = fromParam * perimTotal;
		double toPerim = toParam * perimTotal;
		double perim = 0.;
		double perimLast = 0.;
		for(int i = 1; i <= this->nSpans(); i++) {
			Span sp;
			this->Get(i, sp, true, true);
			perim += sp.length;
			if(fromPerim <= perim && k.m_started == false) {
				// start
				if(FEQ(fromPerim, perim) == true)
					k.Start(sp.p0);
				else {
					double d = fromPerim - perimLast;
					k.Start(sp.MidPerim(d));
				}
			}

			if(perim >= toPerim) {
				// end
				if(FEQ(toPerim, perim) == true)
					k.Add(sp);
				else {
					double d = toPerim - perimLast;
					sp.p1 = sp.MidPerim(d);
					k.Add(sp);
				}				
				break;
			}
			if(k.m_started == true) k.Add(sp);
			perimLast = perim;
		}
		return k;
	}

}