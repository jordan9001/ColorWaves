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

function invertcolor(c) {
    let r = (c & 0xff0000) >> 16;
    let g = (c & 0xff00) >> 8;
    let b = (c & 0xff);
    let inv = ((0xff - r) << 16) + ((0xff - g) << 8) + (0xff - b);
    return inv;
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

CNode.prototype.sort = function() {
    this.points.sort(function(a, b) {
        return a.looppos - b.looppos;
    });
}

CNode.prototype.addpoint = function(color, gpos, lpos) {
    let p = new CNodePoint();
    p.color = color;
    p.gradpos = gpos;
    p.looppos = lpos;
    this.points.push(p);
    this.sort();
    return p;
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
    this.gizmo_rad = 9;
    this.gizmo_sel_rad = this.gizmo_rad * 2;
    this.gizmo_def = "#000000";
    this.gizmo_line = 2;
    this.gizmo_sel = "#eeeeee";

    this.mousex = -1;
    this.mousey = -1;
    this.mousedw = false;

    this.potential_point = null;
    this.sel_pt = null;

    this.selected_node = -1;
    this.selected_point = -1;

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
    dp.color = 0x8000ff;
    dp.gradpos = 0.9;
    dp.looppos = 0.15;
    dn.points.push(dp);
    dp = new CNodePoint();
    dp.color = 0x000000;
    dp.gradpos = 0.9;
    dp.looppos = 0.3;
    dn.points.push(dp);
    dp = new CNodePoint();
    dp.color = 0x00ffff;
    dp.gradpos = 0.81;
    dp.looppos = 0.9;
    dn.points.push(dp);
    this.nodes.push(dn);
}

CControler.prototype.draw = function() {
    // clear the edges
    doccanvas.height = 750;

    // draw the colorgrid
    // Set this higher for higher rez
    let ch = this.canvas.height - (this.ypad*2);
    let cw = this.canvas.width - (this.xpad*2);
    let dp = 2;
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

    // draw interactive stuff
    // first find the mouse
    // if the mouse is over a point, they can drag it, or select it or right click to del
    // if the mouse is over a line, show they can add a point
    // if the mouse is over empty area, show they can add a new node

    this.potential_point = null;
    if (!this.mousedw && this.mousex != -1) { // if we already have one selected with the mouse down, then we can skip this
        this.selected_node = -1;
        this.selected_point = -1;
        for (let i=0; i<this.nodes.length; i++) {
            // check if it is over a point, or over a connecting line
            for (let j=0; j<this.nodes[i].points.length; j++) {
                let pt = this.nodes[i].points[j];
                let ptx = this.xpad + (cw * pt.gradpos);
                let pty = this.ypad + (ch * pt.looppos);
                
                // check if it is over the point
                if (Math.abs(this.mousex - ptx) < this.gizmo_sel_rad && Math.abs(this.mousey - pty) < this.gizmo_rad) {
                    this.selected_node = i;
                    this.selected_point = j;
                    break;
                }
            }
            if (this.selected_node == -1) {
                // check if it is on the line
                let pos = (this.mousey - this.ypad) / ch;
                let pt = this.nodes[i].pointat(pos);
                let ptx = this.xpad + (cw * pt.gradpos);
                if (Math.abs(this.mousex - ptx) < this.gizmo_sel_rad) {
                    this.selected_node = i;
                    this.potential_point = pt;
                }
            }
        }
    }

    
    // draw the ui pieces
    this.ctx.strokeStyle = this.gizmo_def;
    this.ctx.lineWidth = this.gizmo_line;
    this.ctx.strokeRect(this.xpad, this.ypad, cw, ch);

    for (let i=0; i<this.nodes.length; i++) {
        // Draw lines connecting the nodes
        this.ctx.beginPath();
        // move to point at 0
        let pt = this.nodes[i].pointat(0.0);
        let ptx = this.xpad + (cw * pt.gradpos);
        let pty = this.ypad + (ch * pt.looppos);
        this.ctx.moveTo(ptx, pty);
        for (let j=0; j<this.nodes[i].points.length; j++) {
            pt = this.nodes[i].points[j];
            ptx = this.xpad + (cw * pt.gradpos);
            pty = this.ypad + (ch * pt.looppos);
            // draw a line to the next node
            this.ctx.lineTo(ptx, pty);
        }
        // to point at end
        pt = this.nodes[i].pointat(1.0);
        ptx = this.xpad + (cw * pt.gradpos);
        pty = this.ypad + (ch * pt.looppos);
        this.ctx.lineTo(ptx, pty);
        if (this.selected_node == i) {
            this.ctx.strokeStyle = this.gizmo_sel;
            this.ctx.lineWidth = this.gizmo_line * 2;
            this.ctx.stroke();
            this.ctx.strokeStyle = this.gizmo_def;
            this.ctx.lineWidth = this.gizmo_line;
        }
        this.ctx.stroke();
        // draw the circles
        for (let j=0; j<this.nodes[i].points.length; j++) {
            pt = this.nodes[i].points[j];
            ptx = this.xpad + (cw * pt.gradpos);
            pty = this.ypad + (ch * pt.looppos);

            this.ctx.beginPath();
            this.ctx.arc(ptx, pty, this.gizmo_rad, 0, 2*Math.PI);
            this.ctx.closePath();
            this.ctx.fillStyle = num2color(pt.color);
            this.ctx.fill();
            if (pt == this.sel_pt || (this.selected_node == i && this.selected_point == j)) {
                this.ctx.strokeStyle = this.gizmo_sel;
                this.ctx.lineWidth = this.gizmo_line * 3;
                this.ctx.stroke();
                this.ctx.strokeStyle = this.gizmo_def;
                this.ctx.lineWidth = this.gizmo_line;
            }
            this.ctx.stroke();
        }

        // draw the new point
        if (this.potential_point != null && this.selected_node == i) {
            ptx = this.xpad + (cw * this.potential_point.gradpos);
            pty = this.ypad + (ch * this.potential_point.looppos);
            this.ctx.beginPath();
            this.ctx.arc(ptx, pty, this.gizmo_rad, 0, 2*Math.PI);
            this.ctx.closePath();
            this.ctx.fillStyle = num2color(this.potential_point.color);
            this.ctx.fill();
            this.ctx.strokeStyle = this.gizmo_sel;
            this.ctx.lineWidth = this.gizmo_line * 3;
            this.ctx.stroke();
            this.ctx.strokeStyle = this.gizmo_def;
            this.ctx.lineWidth = this.gizmo_line;
            this.ctx.stroke();
        }
    }

    // if there is nothing selected, and we have a mouse in bounds, draw a new node
    if (this.mousedw == false && this.mousex != -1 && this.selected_node == -1) {
        let pp = new CNodePoint();
        pp.color = 0xffffff;
        pp.looppos = (this.mousey - this.ypad) / ch;
        pp.gradpos = (this.mousex - this.xpad) / cw;
        this.potential_point = pp;

        // draw vert line
        this.ctx.beginPath();
        this.ctx.moveTo(this.mousex, this.ypad);
        this.ctx.lineTo(this.mousex, ch + this.ypad);
        this.ctx.stroke();
        // draw circle
        this.ctx.beginPath();
        this.ctx.arc(this.mousex, this.mousey, this.gizmo_rad, 0, 2*Math.PI);
        this.ctx.closePath();
        this.ctx.fillStyle = num2color(pp.color);
        this.ctx.fill();
        this.ctx.stroke();
    }
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
let cslide = document.getElementById('colorslider');
let cpick = document.getElementById('colorpicker');
let cgroup = document.getElementById('colorgroup');
let rpicker = document.getElementById("rinp");
let gpicker = document.getElementById("ginp");
let bpicker = document.getElementById("binp");

function pick_color(hex, hsv, rgb) {
	let r = rgb.r;
	let g = rgb.g;
    let b = rgb.b;
    let c = (r << 16) | (g << 8) | (b);

	cpick.style.backgroundColor = hex;
    cgroup.style.backgroundColor = hex;
    
    if (color_control.sel_pt !== null) {
        color_control.sel_pt.color = c;
        color_control.draw();
    }
	rpicker.value = r;
	gpicker.value = g;
    bpicker.value = b;
}

let colorpicker = ColorPicker(cslide, cpick, pick_color);

function get_color() {
	colorpicker.setRgb({r:rpicker.value, g:gpicker.value, b:bpicker.value});
}

// event listeners
function canvas_rs() {
    doccanvas.width = window.innerWidth * cwidth;

	color_control.draw();
}
window.addEventListener('resize', canvas_rs, false);
canvas_rs(); // do initial draw

// mouse
doccanvas.addEventListener("mousemove", function(evt) {
    let rect = doccanvas.getBoundingClientRect();

    let fx = evt.clientX - (rect.left);
    let fy = evt.clientY - (rect.top);

    if (fx < color_control.xpad || fx > (doccanvas.width - color_control.xpad) || fy < color_control.ypad || fy > (doccanvas.height - color_control.ypad)) {
        fx = -1;
        fy = -1;
    }

    color_control.mousex = fx;
    color_control.mousey = fy;

    //if mouse is down, drag the point with it
    if (color_control.mousedw) {
        let lpos = (fy - color_control.ypad) / (doccanvas.height - (2 * color_control.ypad));
        let gpos = (fx - color_control.xpad) / (doccanvas.width - (2 * color_control.xpad));
        color_control.sel_pt.gradpos = gpos;
        color_control.sel_pt.looppos = lpos;
    }

    color_control.draw();
});

doccanvas.addEventListener("mousedown", function(evt) {
    if (color_control.mousex == -1) {
        return;
    }

    let ns = color_control.selected_node;
    let ps = color_control.selected_point;
    // handle right click
    if (evt.button == 2) {
        // remove any node we have selected
        
        if (ns != -1) {
            if (ps != -1) {
                // remove point
                color_control.nodes[ns].points.splice(ps, 1);
                // if the node has no points left, remove it
                if (color_control.nodes[ns].points.length == 0) {
                    color_control.nodes.splice(ns, 1);
                }
            }
        }
        return;
    }

    if (evt.button != 0) { // only left click from here on
        return;
    }

    color_control.mousedw = true;

    // if we have a point selected, make it the sel_pt;
    if (ns != -1 && ps != -1) {
        color_control.sel_pt = color_control.nodes[ns].points[ps];
        colorpicker.setHex(num2color(color_control.sel_pt.color));
        return;
    }

    // if we have a potential point, now add it
    let pt = color_control.potential_point;
    if (pt !== null) {
        if (ns != -1) {
            pt = color_control.nodes[ns].addpoint(pt.color, pt.gradpos, pt.looppos);
            color_control.nodes[ns].sort();
        } else {
            // make a new node
            let dn = new CNode();
            dn.points.push(pt);
            color_control.nodes.push(dn);
        }
        color_control.sel_pt = pt;
        colorpicker.setHex(num2color(pt.color));
        color_control.potential_point = null;
        return;
    }
});

doccanvas.addEventListener("contextmenu", function(evt) {
    if (evt.preventDefault != undefined) {
        evt.preventDefault();
    }
    if (evt.stopPropagation != undefined) {
        evt.stopPropagation();
    }
    return false;
}, true);

window.addEventListener("mouseup", function(evt) {
    color_control.mousedw = false;
});