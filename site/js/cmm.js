// ColorMeMine interface

// helper functions
function num2color(n) {
    return '#' + n.toString(16).padStart(6, '0');
}

function lerp(a, b, p) {
    return ((b-a) * p) + a;
}

function colorlerp(ac, bc, p) {
    let ra = (ac & 0xff0000);
    let rb = (bc & 0xff0000);
    let r = (((rb - ra) * p) + ra) & 0xff0000;
    let ga = (ac & 0xff00);
    let gb = (bc & 0xff00);
    let g = (((gb - ga) * p) + ga) & 0xff00;
    let ba = (ac & 0xff);
    let bb = (bc & 0xff);
    let b = (((bb - ba) * p) + ba) & 0xff;
    return r+g+b;
}

// our objects
// A node is a color and a percentage across the gradient
function CNodePoint() {
    this.color = 0x000000; // node color
    this.gradpos = 0.0; // percentage across gradient. This will be turned into an index when sent to controller
    this.looppos = 0.0;
}

function CNode() {
    this.points = [];
}

CNode.prototype.pointat = function(looppos) {
    if (this.points.length == 0) {
        return null;
    }

    // see if we can get away with slow way, optimize later if needed
    let ap = null;
    let apd = 0.0
    let bp = null;
    let bpd = 0.0;
    for (let i=0; i<this.points.length; i++) {
        let ipd = (looppos - this.points[i].looppos);
        while (ipd < 0.0) {
            ipd += 1.0;
        } // wrapped around distance
        // a is point earlier of the mid point
        if (ap == null || (ipd <= apd)) {
            apd = ipd;
            ap = this.points[i];
        }
        ipd = 1.0 - ipd;
        // b is point after of the mid point
        if (bp == null || (ipd < bpd)) {
            bpd = ipd;
            bp = this.points[i];
        }
    }

    // now we have 2 points that we can lerp between
    let tn = new CNodePoint();
    let p = apd / (apd + bpd);
    tn.color = colorlerp(ap.color, bp.color, p);
    tn.gradpos = lerp(ap.gradpos, bp.gradpos, p);
    tn.looppos = looppos;

    return tn;
}

// Controls the whole thing
function CControler(can) {
    this.canvas = can;
    this.ctx = can.getContext("2d");

    this.nodes = [];
    this.duration = 5; // total time it takes to loop back

    // constants for how controler looks
    this.xpad = 20;
    this.ypad = 10;

    // DEBUG -- Add some slices
    let dn = new CNode();
    let dp = new CNodePoint();
    dp.color = 0xff0000;
    dp.looppos = 0.0;
    dn.points.push(dp);
    dp = new CNodePoint();
    dp.color = 0x0000ff;
    dp.looppos = 0.5;
    dp.gradpos = 0.4;
    dn.points.push(dp);
    this.nodes.push(dn);

    dn = new CNode();
    dp = new CNodePoint();
    dp.color = 0xffffff;
    dp.gradpos = 0.9;
    dn.points.push(dp);
    dp = new CNodePoint();
    dp.color = 0x00ff00;
    dp.gradpos = 0.9;
    dp.looppos = 0.3;
    dn.points.push(dp);
    dp = new CNodePoint();
    dp.color = 0x00ffff;
    dp.gradpos = 0.9;
    dp.looppos = 0.9;
    dn.points.push(dp);
    this.nodes.push(dn);
}

CControler.prototype.draw = function() {
    // draw the colorgrid
    // Set this higher for higher rez
    let ch = this.canvas.height - (this.ypad*2);
    let cw = this.canvas.width - (this.xpad*2);
    let dp = 10;
    for (let i=0; i<ch; i += dp) {
        let per = i / ch;
        pts = this.getpointsat(per);
        var grd=this.ctx.createLinearGradient(this.xpad, this.ypad + i, this.xpad + cw, this.ypad + i + dp+1)
        for (let n=0; n<pts.length; n++) {
            // build gradient
            grd.addColorStop(pts[n].gradpos, num2color(pts[n].color));
        }
        this.ctx.fillStyle = grd;
        this.ctx.fillRect(this.xpad, this.ypad + i, cw, dp+1);
    }

    // draw the ui pieces
    this.ctx.strokeStyle = "#000000";
    this.ctx.lineWidth = 2;
    this.ctx.strokeRect(this.xpad, this.ypad, cw, ch);

    // draw interactive stuff
    // if mouseover node, draw lines connecting it all the way down
}

CControler.prototype.getpointsat = function(p) {
    nds = [];
    for (let i=0; i<this.nodes.length; i++) {
        let n = this.nodes[i].pointat(p);
        if (n !== null) {
            nds.push(n);
        }
    }
    return nds;
}

// on startup
// get handles to dom stuff
let cwidth = 0.5; // percent width
let doccanvas = document.getElementById("color_canvas");
let color_control = new CControler(doccanvas);

// event listeners
function canvas_rs() {
    doccanvas.width = window.innerWidth * cwidth;
    doccanvas.height = 750;

	color_control.draw();
}
window.addEventListener('resize', canvas_rs, false);
canvas_rs(); // do initial draw
