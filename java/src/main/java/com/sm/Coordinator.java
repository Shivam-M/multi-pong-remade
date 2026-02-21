package com.sm;

import com.sm.protobufs.Pong.*;

public class Coordinator {
    public Coordinator() {
        Ball ball = Ball.newBuilder()
                .setX(0.5f)
                .setY(0.5f)
                .build();

        Query query = Query.getDefaultInstance();
        assert query.equals(Query.newBuilder().build());
    }
}
